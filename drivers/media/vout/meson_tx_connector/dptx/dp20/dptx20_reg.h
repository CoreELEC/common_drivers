/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX20_REG_H__
#define __DPTX20_REG_H__

/* RW   LINK_BW_SET
 *  7:0 Sets the value of the main link bandwidth for the sink device
 *        0x06 = 1.62Gbps
 *        0x08 = 2.16Gbps
 *        0x09 = 2.43Gbps
 *        0x0A = 2.7Gbps
 *        0x0C = 3.24Gbps
 *        0x10 = 4.32Gbps
 *        0x14 = 5.4Gbps
 *        0x1E = 8.1Gbps
 */
#define DPTX20_LINK_BW_SET                     0x000

/* RW   LANE_COUNT_SET
 * 4:0  This field is equivalent to the DPCD register of the same name
 */
#define DPTX20_LANE_COUNT_SET                  0x004

/* RO   ENHANCED_FRAME_EN
 *    0 Enables the enhanced framing mode as supported by the DisplayPort specification.
 */
#define DPTX20_ENHANCED_FRAMING_ENABLE         0x008

/* RW   TRAINING_PATTERN_SET
 *  2:0 Set the link training pattern according to the three-bit code
 *        000 = Training off 001 = Training pattern 1, clock recovery
 *        010 = Training pattern 2, inter-lane alignment and symbol recovery
 *        011 = Training pattern 3, inter-lane alignment and symbol recovery
 *        100 = Training pattern 4, inter-lane alignment and symbol recovery
 */
#define DPTX20_TRAINING_PATTERN_SET            0x00c

/* RW   LINK_QUAL_PATTERN_SET
 * 26:24 LANE_3_PATTERN_SET
 * 18:16 LANE_2_PATTERN_SET
 * 10:8  LANE_1_PATTERN_SET
 *  2:0  LANE_0_PATTERN_SET
 *         000 = Link quality test pattern not transmitted
 *         001 = D10.2 test pattern (unscrambled) transmitted
 *         010 = Symbol Error Rate measurement pattern
 *         011 = PRBS7 transmitted
 *         100 = 80-bit custom pattern
 *         101 = HBR2 eye pattern (CP2520, pattern 1)
 *         110 = CP2520, pattern 2
 *         111 = CP2520, pattern 3
 */
#define DPTX20_LINK_QUAL_PATTERN_SET           0x010

/* RW   SCRAMBLING_DISABLE
 *    0 Set to a 1 to disable the hardware scrambling function. Set to a 0 for normal operation.
 *      This bit must be set during the link training process when transmitting unscrambled
 *      patterns (TP1, TP2, TP3).
 */
#define DPTX20_DISABLE_SCRAMBLING              0x014

/* RW   EDP_CAPABILITY_CONFIG
 *    1 ENABLE_REDUCED_AUX_SYNC
 *        For eDP applications only, enabling this control bit reduces the number of sync
 *        pulses sent during AUX channel transactions from 16 to 8.
 *    0 ALTERNATE_SCRAMBLER_RESET
 *        Set this bit to a 1 to force the transmitter core to use the alternate scrambler
 *        reset value.
 */
#define DPTX20_EDP_CAPABILITY_CONFIG           0x01c

/* RW   HBR2_COMPLIANCE_SCRAMBLER_RESET
 * 15:0 The value of this register should be set to the same value as reported by the
 *      DisplayPort sink device through the DPCD register address 0x0024A-0x0024B
 */
#define DPTX20_HBR2_SCRAMBLER_RESET            0x020

/* RW   DISPLAYPORT_VERSION
 * 5:0  Version number
 *        0x21: DisplayPort 2.1
 *        0x20: DisplayPort 2.0
 *        0x14: DisplayPort 1.4a
 *        0x13: DisplayPort 1.3a
 *        0x12: DisplayPort 1.2a
 *        0x11: DisplayPort 1.1a
 */
#define DPTX20_DISPLAYPORT_VERSION             0x024

/* RW   ALPM_POWER_SET
 *  1:0 Power state sequence request
 *        Writing to these bits will trigger a single transmission of the power state
 *        sequence if one is not currently underway.
 *        default: Store no ALPM data pattern
 *        0x01: Transmit the ML_PHY_SLEEP pattern.
 *        0x10: Transmit the ML_PHY_STANDBY pattern.
 */
#define DPTX20_ALPM_POWER_SET                  0x028

/* RW   LANE_REMAP_CONTROL
 *   19 INVERT_LANE_3: Inverts the state of all bits transmitted to the PHY layer on lane 3.
 *   18 INVERT_LANE_2: Inverts the state of all bits transmitted to the PHY layer on lane 2.
 *   17 INVERT_LANE_1: Inverts the state of all bits transmitted to the PHY layer on lane 1.
 *   16 INVERT_LANE_0: Inverts the state of all bits transmitted to the PHY layer on lane 0
 *    8 REMAP_ENABLE: Set to a 1 to enable the remapping function. When set to a 0, bits 7:0
 *      of the LANE_REMAP control register will be ignored.
 *  7:6 REMAP_LANE_3: Specifies which internal lane number (0-3) will be mapped to the PHY lane 3.
 *  5:4 REMAP_LANE_2: Specifies which internal lane number (0-3) will be mapped to the PHY lane 2.
 *  3:2 REMAP_LANE_1: Specifies which internal lane number (0-3) will be mapped to the PHY lane 1.
 *  1:0 REMAP_LANE_0: Specifies which internal lane number (0-3) will be mapped to the PHY lane 0.
 */
#define DPTX20_LANE_REMAP                      0x02c
    #define LANE_REMAP_EN                      BIT(8)
    #define REMAP_LANE_3_NUM                   6
    #define REMAP_LANE_2_NUM                   4
    #define REMAP_LANE_1_NUM                   2
    #define REMAP_LANE_0_NUM                   0

/* RW   CUSTOM_80BIT_PATTERN_31_0
 * 31:0 Bits 31:0 of the 80-bit custom pattern
 */
#define DPTX20_CUSTOM_80BIT_PATTERN_31_0       0x030
#define DPTX20_CUSTOM_80BIT_PATTERN_63_32      0x034
#define DPTX20_CUSTOM_80BIT_PATTERN_79_64      0x038

/* RW   TRANSMITTER_OUTPUT_ENABLE
 *    0 Set to a 1 after the link output has been configured and the external PHY is ready to
 *      begin operations.
 */
#define DPTX20_TRANSMITTER_ENABLE              0x080

/* WO   SOFT_RESET
 *    4 LINK_SOFT_RESET: Writing a 1 performs a soft reset of the link management section.
 *  3:0 VIDEO_SOFT_RESET: Writing a 1 performs a soft reset of the video capture section for
 *      the respective virtual source.
 */
#define DPTX20_SOFT_RESET                      0x090

/* RW   INPUT_SOURCE_ENABLE
 *    3 VIRTUAL_SOURCE_3_ENABLE: Set to a 1 to enable input source 3 to send valid link symbols
 *      in the MST packet.
 *    2 VIRTUAL_SOURCE_2_ENABLE: Set to a 1 to enable input source 2 to send valid link symbols
 *      in the MST packet.
 *    1 VIRTUAL_SOURCE_1_ENABLE: Set to a 1 to enable input source 1 to send valid link symbols
 *      in the MST packet.
 *    0 VIRTUAL_SOURCE_0_ENABLE: Set to a 1 to enable input source 0 to send valid link symbols
 *      in the MST packet. This bit also applies to the input source in SST mode.
 * Enable these bits in the finial video mode setting
 */
#define DPTX20_SOURCE_ENABLE                   0x094

/* RW   FEC_ENABLE
 *    0 Write to a 1 to enable the FEC logic. This bit is 0 at reset.
 */
#define DPTX20_FEC_ENABLE                      0x098

/* bit0: write 0/1 value to hpd, bit1: pd_over enable */
#define DPTX20_HPD_OVER                        0x09C

/* WO   FORCE_SCRAMBLER_RESET
 *    3 VS3_SCRAMBLER_RESET: Write to a 1 and input source 3 reset directly out of training.
 *      This bit is write only and will always read back as a 0.
 *    2 VS2_SCRAMBLER_RESET: Write to a 1 and input source 2 reset directly out of training.
 *    1 VS1_SCRAMBLER_RESET: Write to a 1 and input source 1 reset directly out of training.
 *    0 VS0_SCRAMBLER_RESET: Write to a 1 and input source 0 reset directly out of training.
 */
#define DPTX20_FORCE_SCRAMBLER_RESET           0x0c0

/* RO   CORE_CAPABILITIES
 *   24 DSC_INCLUDED: Returns a 1 when Display Stream Compression is included in the core.
 *22:20 SECONDARY_SOURCE_COUNT: Reports the number of secondary channels supported. Valid
 *      values are 1 (SST/MST), or 2-4 (MST).
 *18:16 MST_SOURCE_COUNT: Returns a 1 when support for Multi-Stream Transport is not included
 *      in the core implementation. Otherwise, this field reports the maximum number of sources
 *      supported when MST mode is enabled.
 *   12 QUAD_INPUT_SUPPORT_PRESENT: Returns a 1 when support for 4 pixels per cycle is supported
 *      at the video input port. Returns 0 otherwise.
 *   11 FEC_PRESENT: Returns a 1 when support for Forward Error Correction is included in the core.
 *   10 EMBEDDED_PRESENT: Returns 1 when support for embedded DisplayPort is present. Embedded
 *      DisplayPort 1.4b is always supported.
 *    9 AUDIO_PRESENT: Returns 1 when audio functionality is included in the secondary channel
 *      implementation.
 *    8 HDCP_PRESENT: Returns 1 when the HDCP cipher logic is present.
 *  2:0 LANE_COUNT: Indicates the number of lanes implemented in the current configuration of
 *      the core.
 */
#define DPTX20_CORE_FEATURES                   0x0f8

/* RO   CORE_ID
 *31:16 Core ID is fixed at 0x000A
 * 15:0 Core revision level in the form Major [15:8], Minor [7:0].
 */
#define DPTX20_CORE_REVISION                   0x0fc

/* RW   AUX_COMMAND
 *   14 UPDATE_LOCALLY: Set to a 1 to copy the command and address locally when updated.
 *   13 AUX_PHY_WAKE: Set to a 1 to send the AUX_PHY_WAKE signal for embedded DisplayPort
 *      operations. This signal is sent as a standalone operation and is not sent as a part
 *      of a larger AUX transaction.
 *   12 ADDRESS_ONLY: Set to a 1 to initiate an address only request.
 * 11:8 COMMAND: AUX Channel Command. The field is equivalent to the commands defined in
 *      section 2.4.1 of the DisplayPort specification.
 *        0x8 = AUX Write
 *        0x9 = AUX Read
 *        0x0 = I2C over AUX Write
 *        0x4 = I2C over AUX Write, Middle of Transaction bit set
 *        0x1 = I2C over AUX Read
 *        0x5 = I2C over AUX Read, Middle of Transaction bit set
 *        0x2 = I2C over AUX Write Status
 *  3:0 BYTE_COUNT: Specifies the number of bytes to transfer with the current command.
 *       The range of the register is 0 to 15 indicating between 1 and 16 bytes of data.
 */
#define DPTX20_AUX_COMMAND                     0x100
#define AUX_COMMAND_CMD_SHIFT                  8
#define AUX_COMMAND_ADDRESS_ONLY               BIT(12)
#define AUX_COMMAND_BYTES_SHIFT                0
#define AUX_CMD_MASK                           0x0f00
#define AUX_BYTE_CT_MASK                       0x000f

/* RW   AUX_WRITE_FIFO
 *  7:0 Write only AUX Channel byte data. A read from this register is not supported but
 *      will return the last data byte written.
 */
#define DPTX20_AUX_WRITE_FIFO                  0x104

/* RW   AUX_ADDRESS
 * 19:0 Twenty-bit address for the start of the AUX Channel burst.
 */
#define DPTX20_AUX_ADDRESS                     0x108

/* RW   AUX_CLOCK_DIVIDER
 *  9:0 APB clock divider value. The valid range is 10 to 400.
 */
#define DPTX20_AUX_CLOCK_DIVIDER               0x10c

/* RW   AUX_REPLY_TIMEOUT_INTERVAL
 *  8:0 Wait time in microseconds. This register is initialized to a value of 400 upon reset.
 */
#define DPTX20_AUX_REPLY_TIMEOUT_INTERVAL      0x110

/*
 * RW   AUX_REPLY_SYNC_SET
 *  5:0 aux reply sync set: aux sync pattern clk number
 *    6 aux data invert
 */
#define DPTX20_AUX_REPLY_SYNC_SET              0x114

#define TR_DPTX_AUX_GTC_AUTO_CTRL              0x118

#define TR_DPTX_AUX_APB_WR_RD_DONE             0x11c

/* RO   HPD_INPUT_STATE
 *    0 Current state of the external HPD port.
 */
#define DPTX20_HPD_INPUT_STATE                 0x128

/* RO   INTERRUPT_STATE
 *   14 VS3_FIFO_ERROR_EVENT: A 1 indicates an error event in the Video Stream 3 FIFO.
 *   13 VS3_FIFO_OVERFLOW_EVENT: A 1 indicates an overflow event in the Video Stream 3 FIFO.
 *   12 VS2_FIFO_ERROR_EVENT: A 1 indicates an error event in the Video Stream 2 FIFO.
 *   11 VS2_FIFO_OVERFLOW_EVENT: A 1 indicates an overflow event in the Video Stream 2 FIFO.
 *   10 VS1_FIFO_ERROR_EVENT: A 1 indicates an error event in the Video Stream 1 FIFO.
 *    9 VS1_FIFO_OVERFLOW_EVENT: A 1 indicates an overflow event in the Video Stream 1 FIFO.
 *    8 VS0_FIFO_ERROR_EVENT: A 1 indicates an error event in the Video Stream 0 FIFO.
 *    7 VS0_FIFO_OVERFLOW_EVENT: A 1 indicates an overflow event in the Video Stream 0 FIFO.
 *    6 MST_TIMER_EVENT: A 1 indicates that an event has been generated by the MST timer block.
 *    5 HDCP_TIMER_EVENT: A 1 indicates that an event has been generated by the HDCP timer block.
 *    4 GP_TIMER_EVENT: A 1 indicates that an event has been generated by the general purpose
 *      timer block.
 *    3 REPLY_TIMEOUT: A 1 indicates that a reply timeout has occurred
 *    2 REPLY_RECEIVED: A 1 indicates that a reply has been received
 *    1 HPD_IRQ: A 1 indicates that an IRQ interrupt has been received.
 *    0 HPD_EVENT: A 1 indicates that an HPD connect or disconnect event has been detected.
 */
#define DPTX20_INTERRUPT_STATE                 0x130
#define INTERRUPT_STATE_SENDFIFO                    BIT(21)
#define INTERRUPT_STATE_LPM_TIMER_EVENT             BIT(15)
#define INTERRUPT_STATE_SRC3_ERR_EVENT              BIT(14)
#define INTERRUPT_STATE_SRC3_OVF_EVENT              BIT(13)
#define INTERRUPT_STATE_SRC2_ERR_EVENT              BIT(12)
#define INTERRUPT_STATE_SRC2_OVF_EVENT              BIT(11)
#define INTERRUPT_STATE_SRC1_ERR_EVENT              BIT(10)
#define INTERRUPT_STATE_SRC1_OVF_EVENT              BIT(9)
#define INTERRUPT_STATE_SRC0_ERR_EVENT              BIT(8)
#define INTERRUPT_STATE_SRC0_OVF_EVENT              BIT(7)
#define INTERRUPT_STATE_MST_TIMER_EVENT             BIT(6)
#define INTERRUPT_STATE_HDCP_TIMER_EVENT            BIT(5)
#define INTERRUPT_STATE_GP_TIMER_EVENT              BIT(4)
#define INTERRUPT_STATE_REPLY_TIMEOUT               BIT(3)
#define INTERRUPT_STATE_REPLY_RECEIVED              BIT(2)
#define INTERRUPT_STATE_HPD_IRQ                     BIT(1)
#define INTERRUPT_STATE_HPD_EVENT                   BIT(0)
#define INTERRUPT_STATE_MASK                        0x7fff

/* RO   AUX_REPLY_DATA
 *  7:0 AUX reply data from the sink device. Each read advances the internal read pointer.
 */
#define DPTX20_AUX_REPLY_DATA                  0x134

/* RO   AUX_REPLY_CODE
 *  3:0 AUX channel reply code received from the sink device.
 *        0x0 = Native AUX ACK
 *        0x1 = Native AUX NACK
 *        0x2 = Native AUX DEFER
 *        0x0 = I2C over AUX ACK
 *        0x4 = I2C over AUX NACK
 *        0x8 = I2C over AUX DEFER
 */
#define DPTX20_AUX_REPLY_CODE                  0x138
#define AUX_REPLY_CODE_AUX_ACK                 (0)
#define AUX_REPLY_CODE_AUX_NACK                BIT(0)
#define AUX_REPLY_CODE_AUX_DEFER               BIT(1)
#define AUX_REPLY_CODE_I2C_ACK                 (0)
#define AUX_REPLY_CODE_I2C_NACK                BIT(2)
#define AUX_REPLY_CODE_I2C_DEFER               BIT(3)
#define AUX_REPLY_MASK                         0xf

/* RW   AUX_REPLY_COUNT
 *  7:0 Current reply count. The value of this register will automatically roll over after
 *      reaching 255 replies received.
 *    0 write a 1 to this bit to clear the value
 */
#define DPTX20_AUX_REPLY_COUNT                 0x13c

/* RO   INTERRUPT_CAUSE
 * 14:7 SOURCE_ERROR_IRQ: Reserved debug interrupt sources.
 *    6 MST_TIMER_IRQ: The MST internal timer has generated an interrupt.
 *    5 HDCP_TIMER_IRQ: The HDCP internal timer has generated an interrupt.
 *    4 GP_TIMER_IRQ: The internal general-purpose timer has generated an interrupt.
 *    3 REPLY_TIMEOUT: A reply timeout has occurred when the sink has not sent a response 400us
 *      after the transmitter has sent a request.
 *    2 REPLY_RECEIVED: An AUX reply transaction has been detected. This value may be used to
 *      allow a system to process other events while waiting for a response from the sink device.
 *    1 HPD_IRQ: An IRQ framed with the proper timing on the HPD signal has been detected.
 *    0 HPD_EVENT: The core has detected the presence of the HPD signal. This interrupt asserts
 *      immediately after the detection of HPD and after the loss of HPD for 2 msec.
 */
#define DPTX20_INTERRUPT_CAUSE                 0x140
#define INTERRUPT_HPD_IRQ                      BIT(0)
#define INTERRUPT_HPD_EVENT                    BIT(1)
#define INTERRUPT_REPLY_RECEIVED               BIT(2)
#define INTERRUPT_REPLY_TIMEOUT                BIT(3)

/* RW   INTERRUPT_MASK
 * 14:7 SOURCE_ERROR_MASK: Reserved debug interrupt masks. Should always be written to a 1 during
 *      normal operation.
 *    6 MST_TIMER_IRQ_MASK: A 0 in this control bit allows the internal MST timer to cause an
 *      interrupt to be asserted.
 *    5 HDCP_TIMER_IRQ_MASK: A 0 in this control bit allows the internal HDCP timer to cause
 *      an interrupt to be asserted.
 *    4 GP_TIMER_IRQ_MASK: A 0 in this control bit allows the internal general purpose timer
 *      to cause an interrupt to be asserted.
 *    3 REPLY_TIMEOUT_MASK: Write a 0 to this bit to allow reply timeout events to cause an
 *      interrupt to be asserted.
 *    2 REPLY_RECEIVED_MASK: Write a 0 to this bit to allow reply received events to cause an
 *      interrupt.
 *    1 HPD_IRQ_MASK: Write a 0 to this bit to allow HPD_IRQ events to cause an interrupt
 *    0 HPD_EVENT_MASK: Write a 0 to this bit to allow HPD present events to cause an interrupt.
 */
#define DPTX20_INTERRUPT_MASK                  0x144
/*
 * Each interrupt source for the transmitter core may be individually masked.
 * When the corresponding bit in this register is set to 1, the interrupt is
 * masked and will not generate an interrupt for the event.
 *
 * At power up, this register has a value of 0x3F. That is, all interrupt sources are masked.
 */
 #define INTERRUPT_MASK_SENDFIFO_EVENT               BIT(21)
 #define INTERRUPT_MASK_SRC3_ERR_EVENT               BIT(14)
 #define INTERRUPT_MASK_SRC3_OVF_EVENT               BIT(13)
 #define INTERRUPT_MASK_SRC2_ERR_EVENT               BIT(12)
 #define INTERRUPT_MASK_SRC2_OVF_EVENT               BIT(11)
 #define INTERRUPT_MASK_SRC1_ERR_EVENT               BIT(10)
 #define INTERRUPT_MASK_SRC1_OVF_EVENT               BIT(9)
 #define INTERRUPT_MASK_SRC0_ERR_EVENT               BIT(8)
 #define INTERRUPT_MASK_SRC0_OVF_EVENT               BIT(7)
 #define INTERRUPT_MASK_MST_TIMER_IRQ_EVENT          BIT(6)
 #define INTERRUPT_MASK_HDCP_TIMER_IRQ_EVENT         BIT(5)
 #define INTERRUPT_MASK_GP_TIMER_IRQ_EVENT           BIT(4)
 #define INTERRUPT_MASK_REPLY_TIMEOUT_EVENT          BIT(3)
 #define INTERRUPT_MASK_REPLY_RECEIVED_EVENT         BIT(2)
 #define INTERRUPT_MASK_HPD_IRQ_EVENT                BIT(1)
 #define INTERRUPT_MASK_HPD_EVENT                    BIT(0)

/* RO   REPLY_DATA_COUNT
 *  4:0 Total number of data bytes received during the reply phase of the AUX transaction.
 */
#define DPTX20_AUX_REPLY_DATA_COUNT            0x148
#define REPLY_DATA_COUNT_MASK                  0xff

/* RO   AUX_STATUS
 *    4 EXPECT_REPLY: After sending an AUX request, the internal logic sets this flag to indicate
 *      that a reply is expected. This bit is cleared only after a complete reply is received.
 *    3 REPLY_ERROR: When set to a 1, the AUX reply logic has detected an error in the reply to
 *      the most recent AUX transaction. Errors are detected when the pre-charge and sync phases of
 *      the reply last more than 38 cycles instead of the maximum 32. This condition typically
 *      indicates noise on the AUX channel data signals.
 *    2 REQUEST_IN_PROGRESS: The AUX transaction request controller sets this bit to a 1 while
 *      actively transmitting a request on the AUX channel. The bit is set to 0 when the AUX
 *      transaction request controller is idle.
 *    1 REPLY_IN_PROGRESS: The AUX reply detection logic sets this bit to a 1 while receiving a
 *      reply on the AUX channel. The bit is 0 otherwise.
 *    0 REPLY_RECEIVED: This bit is set to 0 when the AUX request controller begins sending bits
 *      on the AUX serial bus. The AUX reply controller sets this bit to 1 when a complete and
 *      valid reply transaction has been received. This bit is cleared when a request transaction
 *      has been initiated by the request controller.
 */
#define DPTX20_AUX_STATUS                      0x14c
#define AUX_STATUS_REPLY_ERROR         BIT(3)
#define AUX_STATUS_REQUEST_IN_PROGRESS BIT(2)
#define AUX_STATUS_REPLY_IN_PROGRESS   BIT(1)
#define AUX_STATUS_REPLY_RECEIVED      BIT(0)

/* RO   AUX_REPLY_CLOCK_WIDTH
 *  9:0 Width of the AUX channel receive clock in APB_CLK cycles.
 */
#define DPTX20_AUX_REPLY_CLOCK_WIDTH           0x150

/* RO   AUX_WAKE_ACK_DETECTED
 *    0 Set to a 1 when the AUX_PHY_WAKE signal has been detected. Clears upon starting the
 *      next AUX transaction.
 */
#define DPTX20_AUX_PHY_WAKE_ACK_DETECTED       0x154

/* RW   GP_HOST_TIMER
 *   31 ENABLE: set to a 1 to enable the timer
 *   30 RELOAD: set to a 1 to automatically reload when the timer reaches zero. When set to 0,
 *      the timer will only run once.
 *   29 INTERRUPT: set to a 1 to generate an interrupt when the timer reaches zero.
 * 23:0 TIMER_VALUE: a write will set the reload value for the timer. A read will return the
 *      current value of the timer.
 */
#define DPTX20_HOST_TIMER                      0x158

/* RW   MST_HOST_TIMER
 *      Refer to GP_HOST_TIMER
 */
#define DPTX20_MST_TIMER                       0x15c

/* RW   LPM_TIMER
 *      Refer to GP_HOST_TIMER
 */
#define DPTX20_LPM_TIMER                       0x160

/* TODO confirm is registers exists */
#define DPTX20_PHY_POST_EMPHASIS_LANE_0		0x240
#define DPTX20_PHY_POST_EMPHASIS_LANE_1		0x244
#define DPTX20_PHY_POST_EMPHASIS_LANE_2		0x24c
#define DPTX20_PHY_POST_EMPHASIS_LANE_3		0x24c

/* RO   PHY_STATUS
 *      specific PHY implementation
 */
#define DPTX20_PHY_STATUS                      0x280

/* RW   HDCP_ENABLE
 *    0 Set to a 1 to enable the HDCP function or to a 0 to disable the function
 */
#define DPTX20_HDCP_ENABLE                     0x400

/* RW   HDCP_MODE
 *  1:0 Set to a 0x1 to select HDCP 1.4 or a 0x2 to select HDCP 2.3.
 *      This register must be set before setting the HDCP_ENABLE bit to a 1.
 */
#define DPTX20_HDCP_MODE                       0x404

/* WO   HDCP_KS_31_0
 * 31:0 Ks value bits 31 down to 0.
 */
#define DPTX20_HDCP_KS_31_0                    0x408

/* WO   HDCP_KS_63_32
 * 31:0 Ks value bits 63 down to 32.
 */
#define DPTX20_HDCP_KS_63_32                   0x40c

/* WO   HDCP_KM_31_0
 * 31:0 Km value bits 31 down to 0 or Ks value bits 95 down to 64.
 */
#define DPTX20_HDCP_KM_31_0                    0x410

/* WO   HDCP_KM_63_32
 * 31:0 Km value bits 55 down to 32 or Ks bits 127 down to 96. In HDCP 1.4 mode,
 *      the upper eight bits of this register are ignored.
 */
#define DPTX20_HDCP_KM_63_32                   0x414

/* RW   HDCP_AN_31_0
 * 31:0 Pseudo-random value An or Rtx, bits 31-0.
 */
#define DPTX20_HDCP_AN_31_0                    0x418
#define DPTX20_HDCP_RTX_31_0                   0x418

/* RW   HDCP_AN_63_32
 * 31:0 Pseudo-random value An or Rtx, bits 63-32.
 */
#define DPTX20_HDCP_AN_63_32                   0x41c
#define DPTX20_HDCP_RTX_63_32                  0x41c

/* RO   HDCP_AUTHENTICATION_IN_PROGRESS
 *    0 Reflects the current state of HDCP 1.4 authentication. This bit will always read
 *      0 when the core is configured for HDCP 2.3 operation.
 */
#define DPTX20_HDCP_AUTH_IN_PROGRESS           0x424

/* RO   HDCP_R0_STATUS
 *   16 R0_AVAILABLE: Indicates that the R0_VALUE field of this register is valid and
 *      contains a value computed from the most recent Km value written to the core.
 *      This bit will remain high after being read and will clear only upon a write to
 *      the HDCP_KM_63_32 register.
 * 15:0 R0_VALUE: Holds the calculated value of R0 to be compared with the R0’ value
 *      calculated by the receiver.
 */
#define DPTX20_HDCP_R0_STATUS                  0x428

/* RW   HDCP_CIPHER_CONTROL
 *    0 HDCP_USE_HW_KM: Enables the internal Km calculation hardware which uses an embedded
 *      Device Private Key memory attached to the transmitter core. The HDCP authentication
 *      logic will recalculate the Km value any time the HDCP_BKSV_63_32 is written. Set to
 *      a 1 to enable the embedded calculation logic using the attached key memory.
 */
#define DPTX20_HDCP_CIPHER_CONTROL             0x42c

/* RW   HDCP_BKSV_31_0
 * 31:0 Least significant 32-bits of the Bksv or Rrx value.
 */
#define DPTX20_HDCP_BKSV_31_0                  0x430
#define DPTX20_HDCP_RRX_31_0                   0x430

/* RW   HDCP_BKSV_63_32
 *  7:0 Most significant 8-bits of the Bksv or Rrx value from the Sink device.
 */
#define DPTX20_HDCP_BKSV_63_32                 0x434
#define DPTX20_HDCP_RRX_63_32                  0x434

/* RO   HDCP_HW_AKSV_31_0
 * 31:0 Least significant 32 bits of the Aksv value read from the attached key memory using
 *      the internal hardware controller.
 */
#define DPTX20_HDCP_AKSV_31_0                  0x438

/* RO   HDCP_HW_AKSV_63_32
 * 31:0 Most significant 32 bits of the Aksv value read from the attached key memory using
 *      the internal hardware controller.
 */
#define DPTX20_HDCP_AKSV_63_32                 0x43c

/* WO   HDCP_LC128_31_0
 * 31:0 Bits 31 down to 0 of the lc128 global constant value.
 */
#define DPTX20_HDCP_LC128_31_0                 0x440

/* WO   HDCP_LC128_63_32
 * 31:0 Bits 63 down to 32 of the lc128 global constant value.
 */
#define DPTX20_HDCP_LC128_63_32                0x444

/* RW   HDCP_LC128_95_64
 * 31:0 Bits 95 down to 64 of the lc128 global constant value.
 */
#define DPTX20_HDCP_LC128_95_64                0x448

/* WO   HDCP_LC128_127_96
 * 31:0 Bits 127 down to 96 of the lc128 global constant value.
 */
#define DPTX20_HDCP_LC128_127_96               0x44c

/* RW   HDCP_REPEATER
 *    0 Set to a 1 to perform authentication as a repeater. Set to a 0 to perform authentication
 *      as a stand-alone source device.
 */
#define DPTX20_HDCP_REPEATER                   0x450

/* RW   HDCP_STREAM_CIPHER_ENABLE
 *    0 Enable the encryption of the main link stream symbols. This bit defaults to a 1 at reset.
 */
#define DPTX20_HDCP_STREAM_CIPHER_ENABLE       0x454

/* RO   HDCP_M0_31_0
 * 31:0 Bits 31 down to 0 of the M0 value calculated during HDCP 1.4 authentication.
 */
#define DPTX20_HDCP_M0_31_0                    0x458

/* RO   HDCP_M0_63_32
 * 31:0 Bits 63 down to 32 of the M0 value calculated during HDCP 1.4 authentication.
 */
#define DPTX20_HDCP_M0_63_32                   0x45c

/* RW   HDCP_AES_INPUT_SELECT
 *  2:0 Input select value according to the following values.
 *        001: normal operation 010: key derivation
 *        100: Ekh(km) Computation
 */
#define DPTX20_HDCP_AES_INPUT_SELECT           0x460

/* RW   HDCP_AES_COUNTER_DISABLE
 *   0 when set to a 1 disables the advancement of the internal counter
 */
#define DPTX20_HDCP_AES_COUNTER_DISABLE        0x464

/* RW   HDCP_AES_COUNTER_ADVANCE
 *    1 Writing a 1 to this register will set the counter state. This register will
 *      automatically clear after writing.
 *    0 Writing a 1 to this register will advance the counter state by 1. This register
 *      will automatically clear after writing.
 */
#define DPTX20_HDCP_AES_COUNTER_ADVANCE        0x468

/* RW   HDCP_ENCRYPTION_CONTROL_FIELD_31_0
 * 31:0 The corresponding bit of this register should be set to a 1 to enable encryption
 *      for the MST transport packet slot.
 */
#define DPTX20_HDCP_ECF_31_0                   0x46c

/* RW   HDCP_ENCRYPTION_CONTROL_FIELD_63_32
 * 31:0 The corresponding bit of this register should be set to a 1 to enable encryption
 *      for the MST transport packet slot.
 */
#define DPTX20_HDCP_ECF_63_32                  0x470

/* RW   HDCP_AES_COUNTER_RESET
 *    0 Writing a 1 to this register bit will reset the internal AES counter to 0.
 *      This bit will automatically clear to a 1 after writing.
 */
#define DPTX20_HDCP_AES_COUNTER_RESET          0x474

/* RW   HDCP_RN_31_0
 * 31:0 Lower 32-bits of the Rn value. This register should be written before attempting
 *      to calculate the value of the derived keys.
 */
#define DPTX20_HDCP_RN_31_0                    0x478

/* RW   HDCP_RN_63_32
 * 31:0 Upper 32-bits of the Rn value. This register should be written before attempting
 *      to calculate the value of the derived keys.
 */
#define DPTX20_HDCP_RN_63_32                   0x47c

/* WO   HDCP_RNG_CIPHER_STORE_AN
 *    0 Writing a 1 to this register will store the current value of the cipher state in
 *      the HDCP_RNG_CIPHER_AN_63_32 and HDCP_RNG_CIPHER_AN_31_0 registers. The value of
 *      this register will automatically clear to a 0 after writing.
 */
#define DPTX20_HDCP_RNG_CIPHER_STORE_AN        0x480

/* RO   HDCP_RNG_CIPHER_AN_31_0
 * 31:0 Least significant 32-bits of the cipher state used as the initial 'AN' value.
 */
#define DPTX20_HDCP_RNG_CIPHER_AN_31_0         0x484

/* RO   HDCP_RNG_CIPHER_AN_63_32
 * 31:0 – Most significant 32-bits of the cipher state used as the initial 'AN' value.
 */
#define DPTX20_HDCP_RNG_CIPHER_AN_63_32        0x488

/* RW   HDCP_HOST_TIMER
 *      Refer to GP_HOST_TIMER
 */
#define DPTX20_HDCP_HOST_TIMER                 0x48c

/* RO   HDCP_ENCRYPTION_STATUS
 *    0 Reports a 1 when encrypted symbols are being transmitted on the DisplayPort link.
 */
#define DPTX20_HDCP_ENCRYPTION_STATUS          0x490

/* RW   HDCP_CONTENT_TYPE_SELECT_31_0
 * 31:0 Set each bit to a 1 for content type 1 or a 0 for content type 0.
 */
#define DPTX20_HDCP_CONTENT_TYPE_SELECT_31_0   0x498

/* RW   HDCP_CONTENT_TYPE_SELECT_63_32
 * 31:0 Set each bit to a 1 for content type 1 or a 0 for content type 0.
 */
#define DPTX20_HDCP_CONTENT_TYPE_SELECT_63_32  0x49c

#define DPTX20_HDCP_BINFO                      0x4a0
#define DPTX20_HDCP_SHA_START                  0x4a4
#define DPTX20_HDCP_KSV_FIFO                   0x4a8
#define DPTX20_HDCP_SHA_STATUS                 0x4ac
#define DPTX20_HDCP_RPT_VH0                    0x4b0
#define DPTX20_HDCP_RPT_VH2                    0x4b8
#define DPTX20_HDCP_RPT_VH1                    0x4b4
#define DPTX20_HDCP_RPT_VH3                    0x4bc
#define DPTX20_HDCP_RPT_VH4                    0x4d0
#define DPTX20_HDCP2_SND_FIFO                  0x4e0
#define DPTX20_HDCP2_RCV_FIFO                  0x4e4
#define DPTX20_HDCP2_HPD_CFG                   0x4e8
#define DPTX20_HDCP2_CFG                       0x4ec
#define DPTX20_HDCP2_FIFO_STATUS               0x4f0
#define DPTX20_HDCP2_TYPE_SET                  0x4f4

/* RW   MST_ENABLE
 *    0 Writing a 1 to this bit enables the MST function.
 */
#define DPTX20_MST_ENABLE                      0x500

/* RO   MST_PID_TABLE_INDEX
 *  5:0 The index has a range of 1 to 63. Index 0 is reserved for the MTP header and cannot
 *      be modified. Index 0 will return a 0 on reads.
 */
#define DPTX20_MST_PID_TABLE_INDEX             0x504

/* RW   MST_PID_TABLE_ENTRY
 *  3:0 Payload Table ID Entry value. This value reflects the index of the video and audio
 *      input source that is allocated to the specified slot in the MTP.
 */
#define DPTX20_MST_PID_TABLE_ENTRY             0x508

/* RW   SST_SOURCE_SELECT
 *  1:0 Selects one of the four possible inputs for transmitting video data in SST mode.
 */
#define DPTX20_SST_SOURCE_SELECT               0x50c

/* RW   MST_ALLOCATION_TRIGGER
 *    0 Writing a 1 starts the ACT event. The bit will read back as 1 until the link has
 *      sent the ACT sequence to the sink device. When the ACT event occurs the link will
 *      be operating with the new payload ID table.
 */
#define DPTX20_MST_ALLOCATION_TRIGGER          0x510

/* RW   MST_PID_TABLE_SELECT
 *    0 Selects the Payload ID table to write.
 */
#define DPTX20_MST_PID_TABLE_SELECT            0x514

/* RW   MST_ACTIVE_PAYLOAD_TABLE
 *    0 Currently active payload ID table while the MST mode is active.
 */
#define DPTX20_MST_ACTIVE_PAYLOAD_TABLE        0x518

/* RO   MST_ACTIVE
 *    3 INPUT_STREAM_3_ACTIVE: Once MST_TX_ENABLE has been set and a vertical sync has been
 *      detected on the stream input port, this bit will be set to a 1.
 *    2 INPUT_STREAM_2_ACTIVE: Identical to INPUT_STREAM_3_ACTIVE.
 *    1 INPUT_STREAM_1_ACTIVE: Identical to INPUT_STREAM_3_ACTIVE.
 *    0 INPUT_STREAM_0_ACTIVE: Identical to INPUT_STREAM_3_ACTIVE.
 */
#define DPTX20_MST_ACTIVE                      0x520

/* RO   MST_LINK_FRAME_COUNT
 *  7:0 Tracks up to 255 link frames. The total time for this counter to roll over is between
 *      404usec (RBR) and 157usec (HBR3). This register will clear on any write.
 */
#define DPTX20_MST_LINK_FRAME_COUNT            0x524

/* RW   MSO_CONFIG
 *    8 MSO_ENABLE: Enable the multi-SST mode for data transmission. Writing a 1 to this bit
 *      enables the MSO function.
 *  7:4 MSO_TIMING_UNLOCKED: Enables the unlocked timing mode when the control bit for input
 *      sources 3:0 is set. The unlocked timing mode is required for the three-segment multi-SST
 *      mode that is not DisplayPort compliant.
 *  1:0 MSO_MODE: Determines the mode for the multi-SST logic. Three DisplayPort compliant modes
 *      are support along with one custom mode. The settings for each mode are shown below.
 *        00 – 2 links of 1 lane each
 *        01 – 2 links of 2 lanes each
 *        10 – 4 links of 1 lane each
 *        11 – 2 links of 1 lane each, 1 link of 2 lanes (not DisplayPort compliant)
 */
#define DPTX20_MSO_CONFIG                      0x528

/* RW   DSC_COMPRESSION_ENABLE
 *    0 Set to a 1 to indicate that the transported video is compressed.
 */
#define DPTX20_DSC_COMPRESSION_ENABLE          0x700

/* RW   DSC_BYTES_PER_CHUNK
 * 15:0 Program to the same value to be used by the chunk_size field of the DSC picture
 *      parameter set.
 */
#define DPTX20_DSC_BYTES_PER_CHUNK             0x704

/* RO   DSC_PPS_BUFFER_BUSY
 *    0 This register is fixed at 0 and cannot be changed.
 */
#define DPTX20_DSC_PPS_BUFFER_BUSY             0x708

/* WO   DSC_PPS_BUFFER_WADDR_RESET
 *    0 Set to a 1 to reset the buffer write address. This bit will always read back as zero.
 */
#define DPTX20_DSC_PPS_BUFFER_WADDR_RESET      0x70c

/* RW   DSC_PPS_BUFFER_WRITE_DATA
 *  7:0 Set to the value of the current PPS table entry. A read will return the last data written.
 */
#define DPTX20_DSC_PPS_BUFFER_WRITE_DATA       0x710

/* RW   DSC_PPS_SDP_CONTROL
 *    1 DSC_PPS_SDP_FRAME_ENABLE: Set this bit to a 1 to enable the transmission of the PPS SDP
 *      during the vertical blanking interval of each frame. This bit is not readable.
 *    0 DSC_PPS_SDP_SEND_NOW: When a 1 is written to this control bit, the internal SDP logic
 *      will transmit the DSC PPS secondary data packet at the next available link time slot.
 */
#define DPTX20_DSC_PPS_SDP_CONTROL             0x714

/* 4 MST Base Offset Addresses */
#define DPTX20_MST_SRC0                        0x000
#define DPTX20_MST_SRC1                        0x200
#define DPTX20_MST_SRC2                        0x400
#define DPTX20_MST_SRC3                        0x600

/* MST Offset Address */
/* Example, use the DPTX20_MST_SRC1 + DPTX20_SRC_SECONDARY_STREAM_ENABLE
 * to access the MST1/SECONDARY_STREAM_ENABLE register
 */

/* RW   SRC0_STREAM_ENABLE
 *    0 SRC0_STREAM_ENABLE: Set this bit to a 1 to enable the output of active data on the
 *      DisplayPort main link. This bit enables source 0 data to be inserted into the packet
 *      in the assigned time slots.
 */
#define DPTX20_SRCX_VIDEO_STREAM_ENABLE        0x800

/* RW   SRC0_SEC_ENABLE
 *    0 SRC0_SEC_ENABLE: Set this to a 1 to enable secondary packets. A value of 0 in this
 *      register will disable secondary data for source 0 and will set the AudioMute flag
 *      in the VB-ID to a 1.
 */
#define DPTX20_SRCX_SECONDARY_STREAM_ENABLE    0x804

/* RW   SRC0_SECONDARY_DATA_WINDOW
 * 11:0 Width of the data valid window. This value should be set using the following equation.
 *      Reg = HBLANK_PERIOD / LINK_SYMBOL_CLOCK_PERIOD * 0.9
 */
#define DPTX20_SRCX_SECONDARY_DATA_WINDOW      0x808

/* RO   SRC0_INPUT_STATUS
 *    3 USER_CONTROL_ODDEVEN: State of the polarity corrected vid_oddeven control input.
 *    2 USER_CONTROL_DEN: Indicates that an active line of video is being received at the
 *      control input. This flag is set when the first pixel of data is received and clears at
 *      the leading edge of the horizontal sync pulse.
 *    1 USER_CONTROL_HSYNC: State of the polarity corrected vid_hsync control input.
 *    0 USER_CONTROL_VSYNC: State of the polarity corrected vid_vsync control input.
 */
#define DPTX20_SRCX_INPUT_STATUS               0x80c

/* RW   SRC0_DATA_CONTROL
 * 15:8 DATA_ACCUMULATION_DELAY: Specifies the number of link clock cycles for the internal
 *      data path controller to wait before sending the first TU of an active line after detecting
 *      that valid data is ready to be transmitted. All seven bits are read-write, however, the
 *      internal logic will only recognize even multiples of 4 for this register. Bits 9 and 8 will
 *      be ignored by the internal framing logic.
 *  5:0 FIFO_ACCUMULATION_DEPTH: Specifies the number of words that must accumulate inside the data
 *      path FIFO before attempting to send a valid data packet on the link. This value is only
 *      valid once per line during the first active data packet.
 */
#define DPTX20_SRCX_DATA_CONTROL               0x810

/* RW   SRC0_COLORIMETRY_OVERRIDE
 *    6 OVERRIDE_ENABLE: Set to a 1 to use the values in this register rather than the values in
 *      the MSA packet. When set to 0, all colorimetry information is determined by the
 *      MISC0/MISC1 fields of the MSA packet.
 *  5:3 COLOR_SPACE_OVERRIDE: Determines the color space to be used when decoding pixel information
 *      from the link. Set to 100 for 4:2:0. Set to 101 for DSC byte mode.
 *  2:0 BITS_PER_COMPONENT_OVERRIDE: Sets the number of bits per component for the selected color
 *      space. Encodings are determined by the selected color space. These bits are equivalent
 *      to bit 7-5 in the MISC0 field.
 */
#define DPTX20_SRCX_COLORIMETRY_OVERRIDE       0x814

/* RW   SRC0_MAIN_STREAM_HTOTAL
 * 15:0 Horizontal line length total in clocks
 */
#define DPTX20_SRCX_MAIN_STREAM_HTOTAL         0x820

/* RW   SRC0_MAIN_STREAM_VTOTAL
 * 15:0 Total number of lines per video frame.
 */
#define DPTX20_SRCX_MAIN_STREAM_VTOTAL         0x824

/* RW   SRC0_MAIN_STREAM_POLARITY
 *    1 VSYNC_POLARITY: polarity of the vertical sync pulse. 0 = active high, 1 = active low
 *    0 HSYNC_POLARITY: polarity of the horizontal sync pulse. 0 = active high, 1 = active low
 */
#define DPTX20_SRCX_MAIN_STREAM_POLARITY       0x828

/* RW   SRC0_MAIN_STREAM_HSWIDTH
 * 14:0 Horizontal sync width in clock cycles
 */
#define DPTX20_SRCX_MAIN_STREAM_HSWIDTH        0x82c

/* RW   SRC0_MAIN_STREAM_VSWIDTH
 * 14:0 Width of the vertical sync in lines.
 */
#define DPTX20_SRCX_MAIN_STREAM_VSWIDTH        0x830

/* RW   SRC0_MAIN_STREAM_HRES
 * 15:0 Number of active pixels per line of the main stream video.
 */
#define DPTX20_SRCX_MAIN_STREAM_HRES           0x834

/* RW   SRC0_MAIN_STREAM_VRES
 * 15:0 Number of active lines of video in the main stream video source.
 */
#define DPTX20_SRCX_MAIN_STREAM_VRES           0x838

/* RW   SRC0_MAIN_STREAM_HSTART
 * 15:0 Horizontal start clock count
 */
#define DPTX20_SRCX_MAIN_STREAM_HSTART         0x83c

/* RW   SRC0_MAIN_STREAM_VSTART
 * 15:0 Vertical start line count.
 */
#define DPTX20_SRCX_MAIN_STREAM_VSTART         0x840

/* RW   SRC0_MAIN_STREAM_MISC0
 *  7:5 BIT_DEPTH: number of bits per color
 *        000 = 6 bits
 *        001 = 8 bits
 *        010 = 10 bits
 *        011 = 12 bits
 *        100 = 16 bits
 *        101, 110, 111 = Reserved
 *    4 YCBCR_COLORIMETRY: colorimetry of the main link video
 *        0 = ITU-R BT601-5
 *        1 = ITU-R BT709-5
 *    3 DYNAMIC_RANGE: color range for each plane
 *        0 = VESA range
 *        1 = CEA range
 *  2:1 COMPONENT_FORMAT: color representation format
 *        00 = RGB 01 = YCbCr 4:2:2
 *        10 = YCbCr 4:4:4
 *        11 = Reserved
 *    0 SYNCHRONOUS_CLOCK: clocking mode for the user data
 *        0 = asynchronous clock
 *        1 = synchronous clock
 */
#define DPTX20_SRCX_MAIN_STREAM_MISC0          0x844

/* RW   SRC0_MAIN_STREAM_MISC1
 *    7 ONE_COMPONENT_COLOR_FORMAT: Set to a 1 to indicate a color format with a single component
 *      including the Y-only and RAW modes.
 *    6 VSC_COLORIMETRY: When set to a 1, this bit indicates that the DP sink device should use
 *      the contents of the VSC packet to determine the Pixel Encoding/Colorimetry information.
 *      The bit encodings of the MISC0/MISC1 fields should be ignored when this bit is set.
 *  5:3 ZERO This field must be set to a 0 for proper operation. Any other value in this field
 *      may cause unpredictable operation.
 *  2:1 STEREO_VIDEO_ATTRIBUTE:
 *        00 = No stereo video transported
 *        01 = Top field or frame is for the RIGHT eye
 *        10 = Reserved
 *        11 = Top field or frame is for the LEFT eye
 *    0 INTERLACED_TOTAL_EVEN:
 *        0 = number of lines per interlaced frame is odd
 *        1 = number of lines per interlaced frame is even
 */
#define DPTX20_SRCX_MAIN_STREAM_MISC1          0x848

/* RW   SRC0_MVID
 * 23:0 unsigned value for MVid. In asynchronous mode, this register is read-only and will
 *      return the value computed by the internal logic based on the input video clock.
 *      In synchronous mode, this register is read-write and should be set to the pixel clock
 *      rate multiplied by 100000. For example, this register should be set to 65000 for a 65MHz
 *      pixel clock or 148500 for a 148.5MHz pixel clock.
 */
#define DPTX20_SRCX_MVID                       0x84c

/* RW   SRC0_TRANSFER_UNIT_CONFIG
 *27:24 FRAC_SYMBOLS_PER_TU: sets the fractional number of valid symbols per TU in units of
 *      1/16th. An accumulator is used to increase the number of data per transaction unit by
 *      1 every 1/N TUs.
 *22:16 SYMBOLS_PER_TU: configures the number of valid symbols to transmit with each transaction
 *      unit. For most applications, this register can be set to a value of 64 (0x40) which allows
 *      the core to automatically manage the TU rate control. For bandwidth limited applications,
 *      this value can be set to any number less than or equal to 64. See Section 2.2.1.4.1 of the
 *      DisplayPort specification for an example that computes this value.
 *  6:2 TRANSFER_UNIT_SIZE: this number must be in the range of 32 to 64 for DisplayPort compliance
 *      and is typically set to a fixed value which depends on the inbound video mode. Larger
 *      values should be used for video modes with slower pixel clock rates. Smaller values should
 *      be used for video modes with higher pixel clock rates. The size of the TU is limited to
 *      integer multiples of 4. Settings that are not a multiple of 4 will be truncated to the
 *      lowest multiple.
 */
#define DPTX20_SRCX_TU_CONFIG                  0x850

/* RW   SRC0_NVID
 * 23:0 unsigned value for NVid. In asynchronous mode, this register is read-only and will return
 *      0x8000. In synchronous mode, this register is read-write and should be set to the link rate
 *      multiplied by 100000. For example, this register should be set to 162000 when operating the
 *      link in 1.62Gbps mode, 270000 when operating the link in 2.7Gbps mode, or 540000 when
 *      operating the link is 5.4Gbps mode.
 */
#define DPTX20_SRCX_NVID                       0x854

/* RW   SRC0_USER_PIXEL_COUNT
 *  2:0 Set to a value of 1 for a single pixel wide interface, 2 for a dual pixel wide interface
 *      or 4 for a quad pixel wide interface.
 */
#define DPTX20_SRCX_USER_PIXEL_COUNT           0x858

/* RW   SRC0_USER_DATA_COUNT
 * 17:0 The equation for setting the value of this register is shown below.
 *      SYMBOL_COUNT = ((HRES * bits per pixel) + 7) / 8
 *      UDC = (SYMBOL_COUNT + lane_count – 1) / lane_count
 */
#define DPTX20_SRCX_USER_DATA_COUNT            0x85c

/* RW   SRC0_MAIN_STREAM_INTERLACED
 *    0 Set to a 1 when transmitting interlaced images or 0 for progressive sources.
 */
#define DPTX20_SRCX_MAIN_STREAM_INTERLACED     0x860

/* RW   SRC0_USER_SYNC_POLARITY
 *    3 USER_ODDEVEN_POLARITY: polarity of the input odd/even field flag.
 *    2 USER_DATA_ENABLE_POLARITY: polarity of the input user data enable signal.
 *    1 USER_VSYNC_POLARITY: polarity of the input user vertical sync pulse.
 *    0 USER_HSYNC_POLARITY: polarity of the input user horizontal sync
 */
#define DPTX20_SRCX_USER_SYNC_POLARITY         0x864

/* RW   SRC0_USER _CONTROL
 *    3 CLEAR_ERROR_STATUS_TOGGLE: When set to a 1, the error status bits are cleared.
 *    2 BYPASS_NO_VIDEO_DELAY: By default, the transmitter SST framing logic waits for
 *      the transmission of 5 NO_VIDEO symbol patterns as required by the DisplayPort
 *      specification. This control bit is intended for debug purposes only and disables
 *      that delay.
 *    1 USER_STREAM_ENABLE_NO_VSYNC: When set to a 1, the main and secondary streams become
 *      active immediately rather than waiting for the next input vertical sync pulse.
 *    0 USER_SPARSE_MODE_ENABLE: A variable number of valid data symbols per transaction
 *      unit are supported when this bit is set to a 1. The value of the SYMBOLS_PER_TU and
 *      FRAC_SYMBOLS_PER_TU fields of the TRANSFER_UNIT_CONFIG register will be ignored when
 *      the sparse mode is enabled.
 */
#define DPTX20_SRCX_USER_CONTROL               0x868

/* RO   SRC0_USER_FIFO_STATUS
 *    1 OVERFLOW: A 1 indicates that the internal user FIFO has detected an overflow condition.
 *      This can occur if the TU transmit data rate is less than the input data rate.
 *    0 ERROR: A 1 indicates that the internal user data error to APB domain.
 */
#define DPTX20_SRCX_USER_FIFO_STATUS           0x86c

/* RO   SRC0_USER_FRAMING_STATUS
 *    1 DATA_UNDERFLOW: The framing state machine will set this bit to a 1 when a TU packet
 *      is being sent and the internal FIFO reports that it is empty.
 *    0 VBI_TIMING_ERROR: This bit will be set when the trailing edge of the input vertical
 *      sync is detected and the internal control logic has not finished transmitting all
 *      of the pixel data from the previous frame.
 */
#define DPTX20_SRCX_FRAMING_STATUS             0x870

#define TR_DPTX_SRC0_INFO_POS01                0x880
#define TR_DPTX_SRC0_INFO_POS23                0x884
#define TR_DPTX_SRC0_INFO_POS45                0x888
#define TR_DPTX_SRC0_INFO_POS67                0x88C
#define TR_DPTX_SRC0_INFO_CTRL                 0x890
#define TR_DPTX_SRC0_VID_INTR_TH               0x894

/* RW   SEC0_AUDIO_ENABLE
 *    1 AUDIO_MUTE: Determines the state of the audio mute bit in the VBID packet.
 *    0 AUDIO_PACKET_ENABLE: Writing a 1 to this bit will globally enable the secondary
 *      channel audio functions. The control registers should be properly set before enabling
 *      the secondary channel audio. Writing a 0 will disable all secondary channel audio packets.
 */
#define DPTX20_SECX_AUDIO_ENABLE               0x900

/* RW   SEC0_AUDIO_INPUT_SELECT
 *  1:0 Audio source select.
 *        00 = I2S
 *        01 = DMA mode
 *        10 = APB host writes
 *        11 = S/PDIF interface (future enhancement)
 */
#define DPTX20_SECX_INPUT_SELECT               0x904

/* RW   SEC0_CHANNEL_COUNT
 *  5:0 Number of audio channels.
 *        0000 = Audio mute set in the VBID field.
 *        0010 = Two channel audio
 *        0011 = Two channel audio with LFE 0110 = 5.1 channel surround sound
 *        1000 = 7.1 channel surround sound
 */
#define DPTX20_SECX_CHANNEL_COUNT              0x908

/* RW   SEC0_SPDIF_CLOCK_COUNT
 *  7:0 Calculated number of APB clock cycles.
 *        count = (1000.0 * apb_clock_freq_in_mhz) / (audio_sample_rate_in_khz * 128.0)
 */
#define DPTX20_SECX_SPDIF_CLOCK_COUNT          0x90c

/* RW   SEC0_INFOFRAME_ENABLE
 *  6:0 Set the corresponding bit to a 0 to disable the transmission of the InfoFrame.
 *      When the bit is set to a 1, the transmitter core will send each enabled packet with
 *      the frequency determined by the SEC_SDP_PACKET_RATE register.
 */
#define DPTX20_SECX_INFOFRAME_ENABLE           0x910

/* RW   SEC0_INFOFRAME_RATE
 *  6:0 Set the bit corresponding to the generic SDP to a 1 to enable continuous transmission
 *      every frame. Set to a 0 to transmit the generic SDP once when the enable bit transitions
 *      from a 0 to a 1. Continuous transmission is defined as one packet sent once per frame
 *      during the vertical blanking interval.
 */
#define DPTX20_SECX_INFOFRAME_RATE             0x914

/* RW   SEC0_MAUD
 * 23:0 Unsigned value computed in the asynchronous clock mode
 */
#define DPTX20_SECX_MAUD                       0x918

/* RW   SEC_NAUD
 * 23:0 Unsigned value computed in the asynchronous clock mode
 */
#define DPTX20_SECX_NAUD                       0x91c

/* RW   SEC0_AUDIO_CLOCK_MODE
 *    1 SEC0_DIRECT_CLOCK_MODE: Set this value to a 1 for DMA inputs used directly. Set this value
 *      to a 0 for DMA values held over from audio clock negative edge used.
 *    0 SEC0_SYNCH_CLOCK_MODE: Set this value to a 1 for synchronous clock mode. Set this value
 *      to a 0 for asynchronous clock mode.
 */
#define DPTX20_SECX_AUDIO_CLOCK_MODE           0x920

/* RW   SEC0_3D_VSC_DATA
 *  3:0 Stereo Interface Method Code
 *  7:4 - Stereo Interface Method-Specific Parameter
 */
#define DPTX20_SECX_3D_VSC_DATA                0x924

/* RW   SEC0_AUDIO_FIFO
 * 31:0 Sample FIFO entry. The internal sample FIFO pointers will automatically increment after
 *      writing. Reading from this register will return the last data written.
 */
#define DPTX20_SECX_AUDIO_FIFO                 0x928

/* RW   SEC0_AUDIO_FIFO_LAST
 *   31 DIRECT_OVERRIDE: Set this value to a 1 to packet read flags based on the read data format.
 *    1 AUDIO_FIFO_LAST: Set this value to a 1 to include the last audio channel data to be
 *      copied from the current sample.
 */
#define DPTX20_SECX_AUDIO_FIFO_LAST            0x92c

/* RO   SEC0_AUDIO_FIFO_READY
 *    0 The direct audio sample FIFO is ready to accept additional samples.
 */
#define DPTX20_SECX_AUDIO_FIFO_READY           0x930

/* RW   SEC0_INFOFRAME_SELECT
 *  2:0 Index of the Infoframe SDP. Valid values are 0 through 6.
 */
#define DPTX20_SECX_INFOFRAME_SELECT           0x934

/* RW   SEC0_SDP_BUFFER_DATA
 *  7:0 Byte value to write to the SDP SRAM. Reading from this register will return the last
 *      value written.
 */
#define DPTX20_SECX_INFOFRAME_DATA             0x938

/* RW   SEC0_TIMESTAMP_INTERVAL
 * 14:0 Number of microseconds to wait after the initial audio timestamp packet to send a
 *      subsequent audio timestamp packet. Timestamp packets will continue to be generated
 *      during the video frame. A value of 0 in this register will cause the transmitter to
 *      send only a single audio timestamp packet during the vertical blanking interval.
 */
#define DPTX20_SECX_TIMESTAMP_INTERVAL         0x93c

/* RW   SEC0_CS_SOURCE_FORMAT
 *  7:4 SOURCE_NUMBER: Four bit code with a unique source number identifier.
 *    3 LINEAR_PCM: 0 for linear PCM samples, 1 for encoded samples
 *  2:0 PCM_AUDIO_FORMAT: Three bit code corresponding to bits 5-3 of the channel status.
 */
#define DPTX20_SECX_CS_SOURCE_FORMAT           0x940

/* RW   SEC0_CS_CATEGORY_CODE
 *  7:0 CATEGORY_CODE: 8-bit category code indicating the type of equipment that generates
 *      the digital audio signal as defined in the relevant annex of IEC60958-3.
 */
#define DPTX20_SECX_CS_CATEGORY_CODE           0x944

/* RW   SEC0_CS_LENGTH_ORIG_FREQ
 *  7:4 SAMPLE_WORD_LENGTH: Reflects the length of each sample word as specified in bits
 *      32-35 of the channel status field in the IEC 60958-3 specification.
 *  3:0 ORIG_SAMPLING_FREQ: Original sampling frequency of the carried audio signal. These
 *      bits reflect the value of bits 39-36 in the IEC60958-3 channel status field.
 */
#define DPTX20_SECX_CS_LENGTH_ORIG_FREQ        0x948

/* RW   SEC0_CS_FREQ_CLOCK_ACCURACY
 *  7:4 SAMPLING_FREQ: Sampling frequency of the current audio signal with the value reflecting
 *      bits 24-27.
 *  3:0 CLOCK_ACCURACY: Clock accuracy code for the current sampling frequency. This code is
 *      equivalent to bits 28-29 of the channel status field.
 */
#define DPTX20_SECX_CS_FREQ_CLOCK_ACCURACY     0x94c

/* RW   SEC0_CS_COPYRIGHT
 *  2:1 CGMS-A: Copy Generation Management System information for the current audio stream.
 *    0 COPYRIGHT: CP-bit for copyright assertion. The mode of alternating the value of this
 *      bit to indicate an unknown state is not supported.
 */
#define DPTX20_SECX_CS_COPYRIGHT               0x950

/* RW   SEC0_AUDIO_CHANNEL_MAP
 *31:28 CHANNEL_7_MAP: remaps the input audio channel 7 to the selected output audio channel (1-8).
 *27:24 CHANNEL_6_MAP
 *23:20 CHANNEL_5_MAP
 *19:16 CHANNEL_4_MAP
 *15:12 CHANNEL_3_MAP
 * 11:8 CHANNEL_2_MAP
 *  7:4 CHANNEL_1_MAP
 *  3:0 CHANNEL_0_MAP
 */
#define DPTX20_SECX_AUDIO_CHANNEL_MAP          0x954

/* RO   SEC0_AUDIO_FIFO_OVERFLOW
 *    0 A value of 1 in this status register indicates that an overflow has occurred. The status
 *      bit is only cleared by setting the SEC_AUDIO_ENABLE bit to a 0.
 */
#define DPTX20_SECX_AUDIO_FIFO_OVERFLOW        0x958

/* RO   SEC0_PACKET_COUNT
 * 11:0 Running count of the number of secondary packets transmitted.
 */
#define DPTX20_SECX_PACKET_COUNT               0x95c

/* RW   SEC0_CS_USER_DATA
 *  7:0 Eight-bit field of user data to be transmitted with the channel status.
 */
#define DPTX20_SECX_CHANNEL_USER_DATA          0x960

/* RW   SEC0_DATA_PACKET_ID
 *  7:0 Unique 8-bit field identifying the secondary data channel stream. This value should
 *      be set to 0x00 for an SST source that maintains only a single audio stream.
 */
#define DPTX20_SECX_DATA_PACKET_ID             0x964

/* RW   SEC0_ADAPTIVE_SYNC_ENABLE
 *    0 Set this bit to a 1 to enable transmission of the adaptive sync SDP. Setting this bit
 *      to a 0 will disable transmission of the packet.
 */
#define DPTX20_SECX_ADAPTIVE_SYNC_ENABLE       0x968

/* RW   SEC0_SPDIF_STATUS_READBACK
 *    3 SPDIF input lane 3 status.
 *    2 SPDIF input lane 2 status.
 *    1 SPDIF input lane 1 status.
 *    0 SPDIF input lane 0 status.
 */
#define DPTX20_SECX_SPDIF_STATUS_READBACK     0x96c

#define TR_DPTX_SEC0_INFOFRAME_ENABLE1         0x970
#define TR_DPTX_SEC0_INFOFRAME_RATE1           0x974

/* RW   SEC0_GTC_COUNT_CONFIG
 *23:16 GTC_COUNT_INT: Integer portion of the GTC accumulator count.
 * 15:0 GTC_COUNT_FRAC: Fractional portion of the GTC accumulator count. This value should be
 *      programmed to the decimal portion of the count times 65536.
 */
#define DPTX20_SECX_GTC_COUNT_CONFIG          0x980

/* RO   SEC_GTC_COMMAND_EDGE
 * 31:0 Internal GTC counter value.
 */
#define DPTX20_SECX_GTC_COMMAND_EDGE          0x984

/* RW   SEC_GTC_AUX_FRAME_SYNC
 * 31:0 Global time code for the AUX frame sync value. This value is automatically transmitted
 *      when a write to DPCD address 0x0015C is performed.
 */
#define DPTX20_SECX_GTC_AUX_FRAME_SYNC        0x988

/* RW   SRC0_EDP_CRC_ENABLE
 *    0 Set to a 1 in eDP PSR or PSR2 mode to enable CRC data calculation.
 */
#define DPTX20_SECX_EDP_CRC_ENABLE            0x990

/* RO   SRC0_EDP_CRC_RED
 * 15:0 Most recent calculated RED CRC value.
 */
#define DPTX20_SECX_EDP_CRC_RED               0x994

/* RO   SRC0_EDP_CRC_GREEN
 * 15:0 Most recent calculated GREEN CRC value.
 */
#define DPTX20_SECX_EDP_CRC_GREEN             0x998

/* RO   SRC0_EDP_CRC_BLUE
 * 15:0 Most recent calculated BLUE CRC value.
 */
#define DPTX20_SECX_EDP_CRC_BLUE              0x99c

/* RW   SRC0_PSR_3D_ENABLE
 *    1 Set to a 1 to enable 3D information to be transmitted in the VSC packet.
 *    0 Set to a 1 to enable the transmission of panel self-refresh information in the VSC
 *      packet information fields.
 */
#define DPTX20_SECX_PSR_3D_ENABLE             0x9a0

/* RW   SRC0_PSR_CONFIG
 *    3 ENABLE_Y_COORD: Set this configuration bit to a 1 to enable the transmission of the Y
 *      coordinate with PSR2 VSC packets. This bit should only be enabled when the connected
 *      sink device supports eDP 1.4a. (DPCD 0x00070 is 3 or greater).
 *    2 INACTIVE_NO_EXIT: When set to a 1, the internal PSR controller will transition to the
 *      inactive state at the next vertical sync without waiting for any additional capture frames.
 *    1 PSR_FRAMES_TO_ACTIVE: Determines the number of input frames to wait between the transition
 *      from the inactive to the active state. Set this bit to a 1 to wait for 2 input frames or a
 *      0 to wait for 1 input frame.
 *    0 PSR_DISABLE_CAPTURE: When set to a 1, this control bit allows the core to automatically
 *      disable the capture ports when the PSR state machine is in the ACTIVE state. Set to a 0 to
 *      control the enable and disable of the capture ports during PSR manually.
 */
#define DPTX20_SECX_PSR_CONFIG                0x9a4

/* RW   SRC0_PSR_STATE
 *    2 SELECTIVE_UPDATE: Set to a 1 to enable the selective update with panel self-refresh.
 *    1 UPDATE_RFB: Set to a 1 to instruct the sink to capture the incoming frame or selective
 *      update region.
 *    0 PSR_ACTIVE: Set to a 1 to enable the panel self-refresh function for the next frame of
 *      captured video input data.
 */
#define DPTX20_SECX_PSR_STATE                 0x9a8

/* RW   SEC_PSR_STATE_INTERNAL
 *  5:0 The value of this register is reserved.
 */
#define DPTX20_SECX_PSR_INTERNAL_STATE        0x9ac

/* RW   PSR2_UPDATE_TOP
 * 15:0 First active line for selective update. The first line of active video is defined as
 *      line 0 of the frame.
 */
#define DPTX20_SECX_PSR2_UPDATE_TOP           0x9b0

/* RW   PSR2_UPDATE_BOTTOM
 * 15:0 Last active line for selective update. The first line of active video is defined as
 *      line 0 of the frame.
 */
#define DPTX20_SECX_PSR2_UPDATE_BOTTOM        0x9b4

/* RW   PSR2_UPDATE_LEFT
 * 15:0 First active pixel for selective update. The first pixel of active video is defined as
 *      pixel 0 of the line.
 */
#define DPTX20_SECX_PSR2_UPDATE_LEFT          0x9b8

/* RW   PSR2_UPDATE_WIDTH
 * 15:0 Width of the selective update region in pixels.
 */
#define DPTX20_SECX_PSR2_UPDATE_WIDTH         0x9bc

#endif
