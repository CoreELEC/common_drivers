/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VDIN_A9_REGS_H
#define __VDIN_A9_REGS_H

/* a9 afbce0 and afbce1 */
#define VDIN_AFBC_OFFSET_A9          0x50

#define VDIN0_A9_AFBCE_FORMAT                         ((0x4160))
//Bit 31:15       reserved
//Bit 14:12       reg_burst_length_add_value      // unsigned ,   RW, default = 2
//Bit 11          reg_ofset_burst4_en     \n unsigned ,   RW, default = 0
//Bit 10          reg_burst_length_add_en         // unsigned ,   RW, default = 0
//Bit  9: 8       reg_format_mode                 // unsigned ,   RW, default = 2
	//data format 0 : YUV444  1:YUV422  2:YUV420  3:RGB
//Bit  7: 4       reg_compbits_c                  // unsigned ,   RW, default = 10  chroma bitwidth
//Bit  3: 0       reg_compbits_y                  // unsigned ,   RW, default = 10  luma bitwidth
#define VDIN0_A9_AFBCE_MODE_EN                        ((0x4161))
//Bit 31:28       reserved
//Bit 27:26       reserved
//Bit 25          reg_adpt_interleave_ymode       // unsigned ,   RW, default = 0
	//force 0 to disable it: no  HW implementation
//Bit 24          reg_adpt_interleave_cmode       // unsigned ,   RW, default = 0
	//force 0 to disable it: not HW implementation
//Bit 23          reg_adpt_yinterleave_luma_ride  // unsigned ,   RW, default = 1
	//vertical interleave piece luma reorder ride    0: no reorder ride  1: w/4 as ride
//Bit 22          reg_adpt_yinterleave_chrm_ride  // unsigned ,   RW, default = 1
	//vertical interleave piece chroma reorder ride  0: no reorder ride  1: w/2 as ride
//Bit 21          reg_adpt_xinterleave_luma_ride  // unsigned ,   RW, default = 1
	//vertical interleave piece luma reorder ride    0: no reorder ride  1: w/4 as ride
//Bit 20          reg_adpt_xinterleave_chrm_ride  // unsigned ,   RW, default = 1
	//vertical interleave piece chroma reorder ride  0: no reorder ride  1: w/2 as ride
//Bit 19          reserved
	//disable order mode0~6: each mode with one  disable bit: 0: no disable  1: disable
//Bit 18          reg_disable_order_mode_i_6      // unsigned ,   RW, default = 0
//Bit 17          reg_disable_order_mode_i_5      // unsigned ,   RW, default = 0
//Bit 16          reg_disable_order_mode_i_4      // unsigned ,   RW, default = 0
//Bit 15          reg_disable_order_mode_i_3      // unsigned ,   RW, default = 0
//Bit 14          reg_disable_order_mode_i_2      // unsigned ,   RW, default = 0
//Bit 13          reg_disable_order_mode_i_1      // unsigned ,   RW, default = 0
//Bit 12          reg_disable_order_mode_i_0      // unsigned ,   RW, default = 0
//Bit 11          reserved
//Bit 10          reg_minval_yenc_en              // unsigned ,   RW, default = 0
	//force disable  final decision to remove this ws 1% performance loss
//Bit  9          reg_16x4block_enable            // unsigned ,   RW, default = 0
	//block as mission  but permit 16x4 block
//Bit  8          reg_uncompress_split_mode       // unsigned ,   RW, default = 0
	//0: no split  1: split
//Bit  7: 6       reserved
//Bit  5          reg_input_padding_uv128         // unsigned ,   RW, default = 0
//	input picture 32x4 block gap mode: 0:  pad uv=0  1: pad uv=128
//Bit  4          reg_dwds_padding_uv128          // unsigned ,   RW, default = 0
	//downsampled image for double write 32x gap mode: 0:  pad uv=0  1: pad uv=128
//Bit  3: 1       reg_force_order_mode_value      // unsigned ,   RW, default = 0
	//force order mode 0~7
//Bit  0          reg_force_order_mode_en         // unsigned ,   RW, default = 0
	//force order mode enable: 0: no force  1: forced to force_value
#define VDIN0_A9_AFBCE_DWSCALAR                       ((0x4162))
//Bit 31: 8       reserved
//Bit  7: 6       reg_dwscalar_w0                 // unsigned ,   RW, default = 3
	//horizontal 1st step scalar mode: 0: 1:1 no scalar  1: 2:1 data drop ((0 2 4  6))
	//pixel kept  2: 2:1 data drop (1  3  5 7..)) pixels kept  3: avg
//Bit  5: 4       reg_dwscalar_w1                 // unsigned ,   RW, default = 0
	//horizontal 2nd step scalar mode: 0: 1:1 no scalar  1: 2:1 data drop ((0 2 4  6))
	//pixel kept  2: 2:1 data drop (1  3  5 7..)) pixels kept  3: avg
//Bit  3: 2       reg_dwscalar_h0                 // unsigned ,   RW, default = 2
	//vertical 1st step scalar mode: 0: 1:1 no scalar  1: 2:1 data drop ((0 2 4  6))
	//pixel kept  2: 2:1 data drop (1  3  5 7..)) pixels kept  3: avg
//Bit  1: 0       reg_dwscalar_h1                 // unsigned ,   RW, default = 3
	//vertical 2nd step scalar mode: 0: 1:1 no scalar  1: 2:1 data drop ((0 2 4  6))
	//pixel kept  2: 2:1 data drop (1  3  5 7..)) pixels kept  3: avg
#define VDIN0_A9_AFBCE_DEFCOLOR_1                     ((0x4163))
//Bit 31:24       reserved
//Bit 23:12       reg_enc_defaultcolor_3
// unsigned ,   RW, default = 4095  Picture wise default color value in [Y Cb Cr]
//Bit 11: 0       reg_enc_defaultcolor_0
// unsigned ,   RW, default = 4095  Picture wise default color value in [Y Cb Cr]
#define VDIN0_A9_AFBCE_DEFCOLOR_2                     ((0x4164))
//Bit 31:24       reserved
//Bit 23:12       reg_enc_defaultcolor_2
// unsigned ,   RW, default = 4095  wise default color value in [Y Cb Cr]
//Bit 11: 0       reg_enc_defaultcolor_1
// unsigned ,   RW, default = 4095  wise default color value in [Y Cb Cr]
#define VDIN0_A9_AFBCE_QUANT_ENABLE                   ((0x4165))
//Bit 31:12       reserved
//Bit 11          reg_quant_expand_en_1
// unsigned ,   RW, default = 0  enable for quantization value expansion
//Bit 10          reg_quant_expand_en_0
// unsigned ,   RW, default = 0  enable for quantization value expansion
//Bit  9: 8       reg_bcleav_ofst
//   signed ,   RW, default = 0  bcleave ofset to get lower range  especially under lossy
//for v1/v2  x=0 is equivalent  default = -1
//Bit  7: 5       reserved
//Bit  4          reg_quant_enable_1
// unsigned ,   RW, default = 0  enable for quant to get some lossy
//Bit  3: 1       reserved
//Bit  0          reg_quant_enable_0
// unsigned ,   RW, default = 0  enable for quant to get some lossy
#define VDIN0_A9_AFBCE_IQUANT_LUT_1                   ((0x4166))
//Bit 31          reserved
//Bit 30:28       reg_iquant_yclut_0_11
// unsigned ,   RW, default = 0  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 27          reserved
//Bit 26:24       reg_iquant_yclut_0_10
// unsigned ,   RW, default = 1  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 23          reserved
//Bit 22:20       reg_iquant_yclut_0_9
// unsigned ,   RW, default = 2  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 19          reserved
//Bit 18:16       reg_iquant_yclut_0_8
// unsigned ,   RW, default = 3  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_0_7
// unsigned ,   RW, default = 4  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_0_6
// unsigned ,   RW, default = 5  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_0_5
// unsigned ,   RW, default = 5  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_0_4
// unsigned ,   RW, default = 4  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
#define VDIN0_A9_AFBCE_IQUANT_LUT_2                   ((0x4167))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_0_3g
// unsigned ,   RW, default = 3  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_0_2
// unsigned ,   RW, default = 2  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_0_1
// unsigned ,   RW, default = 1  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_0_0
// unsigned ,   RW, default = 0  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
#define VDIN0_A9_AFBCE_IQUANT_LUT_3                   ((0x4168))
//Bit 31          reserved
//Bit 30:28       reg_iquant_yclut_1_11
// unsigned ,   RW, default = 0  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 27          reserved
//Bit 26:24       reg_iquant_yclut_1_10
// unsigned ,   RW, default = 1  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 23          reserved
//Bit 22:20       reg_iquant_yclut_1_9
// unsigned ,   RW, default = 2  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 19          reserved
//Bit 18:16       reg_iquant_yclut_1_8
// unsigned ,   RW, default = 3  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_1_7
// unsigned ,   RW, default = 4  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_1_6
// unsigned ,   RW, default = 5  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_1_5
// unsigned ,   RW, default = 5  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_1_4
// unsigned ,   RW, default = 4  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
#define VDIN0_A9_AFBCE_IQUANT_LUT_4                   ((0x4169))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_1_3
// unsigned ,   RW, default = 3  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_1_2
// unsigned ,   RW, default = 2  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_1_1
// unsigned ,   RW, default = 1  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_1_0
// unsigned ,   RW, default = 0  quantization lut for mintree leavs  iquant=2^lut(bc_leav_q+1))
#define VDIN0_A9_AFBCE_RQUANT_LUT_1                   ((0x416a))
//Bit 31          reserved
//Bit 30:28       reg_rquant_yclut_0_11           //
// unsigned ,   RW, default = 5  quantization lut for bctree leavs  quant=2^lut(bc_leav_r+1))
	//can be calculated from iquant_yclut(fw_setting))
//Bit 27          reserved
//Bit 26:24       reg_rquant_yclut_0_10           // unsigned ,   RW, default = 5
//Bit 23          reserved
//Bit 22:20       reg_rquant_yclut_0_9            // unsigned ,   RW, default = 4
//Bit 19          reserved
//Bit 18:16       reg_rquant_yclut_0_8            // unsigned ,   RW, default = 4
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_0_7            // unsigned ,   RW, default = 3
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_0_6            // unsigned ,   RW, default = 3
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_0_5            // unsigned ,   RW, default = 2
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_0_4            // unsigned ,   RW, default = 2
#define VDIN0_A9_AFBCE_RQUANT_LUT_2                   ((0x416b))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_0_3            // unsigned ,   RW, default = 1
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_0_2            // unsigned ,   RW, default = 1
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_0_1            // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_0_0            // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_RQUANT_LUT_3                   ((0x416c))
//Bit 31          reserved
//Bit 30:28       reg_rquant_yclut_1_11           // unsigned ,   RW, default = 5
//quantization lut for bctree leavs  quant=2^lut(bc_leav_r+1))
//can be calculated from iquant_yclut(fw_setting))
//Bit 27          reserved
//Bit 26:24       reg_rquant_yclut_1_10           // unsigned ,   RW, default = 5
//Bit 23          reserved
//Bit 22:20       reg_rquant_yclut_1_9            // unsigned ,   RW, default = 4
//Bit 19          reserved
//Bit 18:16       reg_rquant_yclut_1_8            // unsigned ,   RW, default = 4
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_1_7            // unsigned ,   RW, default = 3
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_1_6            // unsigned ,   RW, default = 3
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_1_5            // unsigned ,   RW, default = 2
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_1_4            // unsigned ,   RW, default = 2
#define VDIN0_A9_AFBCE_RQUANT_LUT_4                   ((0x416d))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_1_3            // unsigned ,   RW, default = 1
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_1_2            // unsigned ,   RW, default = 1
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_1_1            // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_1_0            // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_YUV_FORMAT_CONV_MODE           ((0x416e))
//Bit 31: 8       reserved
//Bit  7          reserved
//Bit  6: 4       reg_444to422_mode               // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_422to420_mode               // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_LOSS_CTRL                      ((0x416f))
//Bit 31         reg_fix_cr_en
// unsigned ,    RW, default = 1  enable of fix CR
//Bit 30         reg_rc_en                        // unsigned ,    RW, default = 0  enable of rc
//Bit 29         reg_write_qlevel_mode
// unsigned ,    RW, default = 1  qlevel write mode
//Bit 28         reg_debug_qlevel_en              // unsigned ,    RW, default = 0  debug en
//Bit 27:20      reserved
//Bit 19:18      reserved
//Bit 17:16      reg_cal_bit_early_mode
// unsigned ,    RW, default = 2  early mode, RTL fix 2
//Bit 15:12      reg_quant_diff_root_leave
// unsigned ,    RW, default = 2  diff value of bctroot and bcleave
//Bit 11: 8      reg_debug_qlevel_value
// unsigned ,    RW, default = 0  debug qlevel value
//Bit  7: 4      reg_debug_qlevel_max_0
// unsigned ,    RW, default = 10  debug qlevel value
//Bit  3: 0      reg_debug_qlevel_max_1
// unsigned ,    RW, default = 10  debug qlevel value
#define VDIN0_A9_AFBCE_LOSS_BURST_NUM                 ((0x4170))
//Bit 31:29      reserved
//Bit 28:24      reg_block_burst_num_0
// unsigned ,    RW, default = 5  the num of burst of block
//Bit 23:21      reserved
//Bit 20:16      reg_block_burst_num_1
// unsigned ,    RW, default = 5  the num of burst of block
//Bit 15:13      reserved
//Bit 12: 8      reg_block_burst_num_2
// unsigned ,    RW, default = 5  the num of burst of block
//Bit  7: 5      reserved
//Bit  4: 0      reg_block_burst_num_3
// unsigned ,    RW, default = 5  the num of burst of block
#define VDIN0_A9_AFBCE_LOSS_RC                        ((0x4171))
//Bit 31:29      reserved
//Bit 28         reg_rc_fifo_margin_mode
// unsigned ,    RW, default = 1  enable of fifo margin mode
//Bit 27:24      reg_rc_1stbl_xbdgt
// unsigned ,    RW, default = 0  extra bit budget for the first block in each line
//Bit 23:20      reg_rc_1stln_xbdgt
// unsigned ,    RW, default = 0  extra bit budget for the first line
//Bit 19:18      reserved
//Bit 17:16      reg_rc_qlevel_chk_mode
// unsigned ,    RW, default = 0  0: max qlevel 1: avg qleve 2:max luma qlevel 3 : avg luma qlevel
//Bit 15: 8      reg_fix_cr_subb_bit_adj_0
// signed ,    RW, default = 0  luma each sub block add bits normal >=0
//Bit  7: 0      reg_fix_cr_subb_bit_adj_1
// signed ,    RW, default = 0  chroma each sub block minus bits normal <= 0
#define VDIN0_A9_AFBCE_LOSS_RC_FIFO_THD               ((0x4172))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_0
// unsigned ,    RW, default = 8
//threshold of fifo level to guard the rc loop by adding delta to burst
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_fifo_margin_thd_1         // unsigned ,    RW, default = 16
#define VDIN0_A9_AFBCE_LOSS_RC_FIFO_BUGET             ((0x4173))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_2         // unsigned ,    RW, default = 32
//Bit 15:12      reg_rc_fifo_margin_budget_0
// unsigned ,    RW, default = 2  delta of fifo level to guard the rc loop
//Bit 11: 8      reg_rc_fifo_margin_budget_1      // unsigned ,    RW, default = 1
//Bit  7: 4      reg_rc_fifo_margin_budget_2      // unsigned ,    RW, default = 0
//Bit  3: 0      reg_rc_max_add_budget            // unsigned ,    RW, default = 2
//the max add burst num
#define VDIN0_A9_AFBCE_LOSS_RC_ACCUM_THD_0            ((0x4174))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_2
// unsigned ,    RW, default = 8  threshold of accum to guard the rc loop by adding delta to burst
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_2_1
// unsigned ,    RW, default = 8  threshold of accum to guard the rc loop by adding delta to burst
#define VDIN0_A9_AFBCE_LOSS_RC_ACCUM_THD_1            ((0x4175))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_0
// unsigned ,    RW, default = 8  threshold of accum to guard the rc loop by adding delta to burst
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_2
// unsigned ,   RW, default = 128 threshold of accum to guard the rc loop by adding delta to burst
#define VDIN0_A9_AFBCE_LOSS_RC_ACCUM_THD_2            ((0x4176))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_1_1
// unsigned ,    RW, default = 64 threshold of accum to guard the rc loop by adding delta to burst
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_0
// unsigned ,    RW, default = 4  threshold of accum to guard the rc loop by adding delta to burst
#define VDIN0_A9_AFBCE_LOSS_RC_ACCUM_THD_3            ((0x4177))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_0_2
// unsigned ,    RW, default = 256 threshold of accum to guard the rc loop by adding delta to burst
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_0_1
// unsigned ,    RW, default = 128 threshold of accum to guard the rc loop by adding delta to burst
#define VDIN0_A9_AFBCE_LOSS_RC_ACCUM_BUGET_0          ((0x4178))
//Bit 31:20      reg_rc_accum_margin_thd_0_0
// unsigned ,    RW, default = 0  threshold of accum to guard the rc loop by adding delta to burst
//Bit 19:16      reserved
//Bit 15:12      reg_rc_accum_margin_budget_2_2
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 11: 8      reg_rc_accum_margin_budget_2_1
// unsigned ,    RW, default = 0
//Bit  7: 4      reg_rc_accum_margin_budget_2_0
// unsigned ,    RW, default = 0
//Bit  3: 0      reg_rc_accum_margin_budget_1_2
// unsigned ,    RW, default = 2
#define VDIN0_A9_AFBCE_LOSS_RC_ACCUM_BUGET_1          ((0x4179))
//Bit 31:28      reg_rc_accum_margin_budget_1_1
// unsigned ,    RW, default = 1
//Bit 27:24      reg_rc_accum_margin_budget_1_0
// unsigned ,    RW, default = 1
//Bit 23:20      reg_rc_accum_margin_budget_0_2
// unsigned ,    RW, default = 2
//Bit 19:16      reg_rc_accum_margin_budget_0_1   // unsigned ,    RW, default = 1
//Bit 15:12      reg_rc_accum_margin_budget_0_0   // unsigned ,    RW, default = 1
//Bit 11: 8      reserved
//Bit  7: 4      reg_rc_qlevel_chk_th_0
// unsigned ,    RW, default = 5  threshold of qlevel for adjust burst based on accum
//Bit  3: 0      reg_rc_qlevel_chk_th_1
// unsigned ,    RW, default = 4  threshold of qlevel for adjust burst based on accum
#define VDIN0_A9_AFBCE_LOSS_RO_ERROR_L_0              ((0x417a))
//Bit 31: 0      ro_error_acc_mode0_l_0           // unsigned ,    RO, default = 0  square
#define VDIN0_A9_AFBCE_LOSS_RO_COUNT_0                ((0x417b))
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_0             // unsigned ,  RO, default = 0  mode num contour
#define VDIN0_A9_AFBCE_LOSS_RO_ERROR_L_1              ((0x417c))
//Bit 31: 0      ro_error_acc_mode0_l_1           // unsigned ,    RO, default = 0  square
#define VDIN0_A9_AFBCE_LOSS_RO_COUNT_1                ((0x417d))
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_1             // unsigned ,  RO, default = 0  mode num contour
#define VDIN0_A9_AFBCE_LOSS_RO_ERROR_L_2              ((0x417e))
//Bit 31: 0      ro_error_acc_mode0_l_2           // unsigned ,    RO, default = 0  square
#define VDIN0_A9_AFBCE_LOSS_RO_COUNT_2                ((0x417f))
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_2             // unsigned ,  RO, default = 0  mode num contour
#define VDIN0_A9_AFBCE_LOSS_RO_ERROR_H_0              ((0x4180))
//Bit 31:16      ro_error_acc_mode0_h_2           // unsigned ,    RO, default = 0  square
//Bit 15: 0      ro_error_acc_mode0_h_1           // unsigned ,    RO, default = 0  square
#define VDIN0_A9_AFBCE_LOSS_RO_ERROR_H_1              ((0x4181))
//Bit 31:16      reserved
//Bit 15: 0      ro_error_acc_mode0_h_0           // unsigned ,    RO, default = 0  square
#define VDIN0_A9_AFBCE_LOSS_RO_MAX_ERROR_0            ((0x4182))
//Bit 31:28      reserved
//Bit 27:16      ro_max_error_mode0_2             // unsigned ,    RO, default = 0  error
//Bit 15:12      reserved
//Bit 11: 0      ro_max_error_mode0_1             // unsigned ,    RO, default = 0  error
#define VDIN0_A9_AFBCE_LOSS_RO_MAX_ERROR_1            ((0x4183))
//Bit 31:12      reserved
//Bit 11: 0      ro_max_error_mode0_0             // unsigned ,    RO, default = 0  error
#define VDIN0_A9_AFBCE_ENABLE                         ((0x4184))
//Bit 31:20       reg_gclk_ctrl
// unsigned ,   RW, default = 0  12'hfff:always open 12'h000:gating clock
//Bit 19:16       reg_afbce_sync_sel
// unsigned ,   RW, default = 0  4'hf:registers sync by vsync 4'h0:registers don't sync
//Bit 15:14       reserved
//Bit 13          reg_enc_rst_mode
// unsigned ,   RW, default = 0  1:soft reset by write pulse into registers VDIN0_A9_AFBCE_MODE[29]
//0:auto reset by vsync
//Bit 12          reg_enc_en_mode
// unsigned ,   RW, default = 0  1:start afbce by write a pulse into VDIN0_A9_AFBCE_ENABLE[0]
//0:auto start several lines after vsync
//Bit 11: 9       reserved
//Bit  8          reg_enc_enable
// unsigned ,   RW, default = 0  1:afbce enable 0:afbce disable
//Bit  7: 1       reserved
//Bit  0          reg_pls_enc_frm_start
// unsigned ,   W1C, default = 0  reg_pls_enc_frm_start pulse
#define VDIN0_A9_AFBCE_MODE                           ((0x4185))
//Bit 31:29       reg_soft_rst                    // unsigned ,   RW, default = 0
//active high bit0:async reset afbce by write a pulse bit1:async reset afbce
//after vsync bit2:sync reset after vsync
//Bit 28          reg_axi_chk_sel                 // unsigned ,   RW, default = 0
//Bit 27:26       reg_rev_mode
// unsigned ,   RW, default = 0  reverse mode bit0:x reverse 1:y reverse
//Bit 25:24       reg_mif_urgent
// unsigned ,   RW, default = 3  info mif and data mif urgent
//Bit 23          reg_abort_wstrb                 // unsigned ,   RW, default = 0
//Bit 22:16       reg_hold_line_num
// unsigned ,   RW, default = 4  0: burst1 1:burst2 2:burst4
//Bit 15:14       reg_burst_mode
// unsigned ,   RW, default = 2  0: burst1 1:burst2 2:burst4
//Bit 13: 1       reserved
//Bit  0          reg_fmt444_comb
// unsigned ,   RW, default = 0  0: 444 8bit comb mode
#define VDIN0_A9_AFBCE_SIZE_IN                        ((0x4186))
//Bit 31:16       reg_hsize_bg
// unsigned ,   RW, default = 1920  pic horz background size in  unit pixel
//Bit 15: 0       reg_vsize_bg
// unsigned ,   RW, default = 1080  pic vert background size in  unit pixel
#define VDIN0_A9_AFBCE_BLK_SIZE_IN                    ((0x4187))
//Bit 31:16       reg_hblk_size
// unsigned ,   RW, default = 60  pic horz size in block
//Bit 15: 0       reg_vblk_size
// unsigned ,   RW, default = 270  pic vert size in block
#define VDIN0_A9_AFBCE_HEAD_BADDR                     ((0x4188))
//Bit 31: 0       reg_head_baddr
// unsigned ,   RW, default = 32'h00  head address
#define VDIN0_A9_AFBCE_MIF_SIZE                       ((0x4189))
//Bit 31:30       reserved
//Bit 29:28       reg_ddr_blk_size                // unsigned ,   RW, default = 1
//Bit 27          reserved
//Bit 26:24       reg_cmd_blk_size                // unsigned ,   RW, default = 3
//Bit 23:16       reg_uncmp_size                  // unsigned ,   RW, default = 20
//Bit 15: 0       reg_mmu_page_size               // unsigned ,   RW, default = 4096
#define VDIN0_A9_AFBCE_PIXEL_IN_HOR_SCOPE             ((0x418a))
//Bit 31:16       reg_enc_win_end_h               // unsigned ,   RW, default = 1919
//Bit 15: 0       reg_enc_win_bgn_h               // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_PIXEL_IN_VER_SCOPE             ((0x418b))
//Bit 31:16       reg_enc_win_end_v               // unsigned ,   RW, default = 1079
//Bit 15: 0       reg_enc_win_bgn_v               // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_CONV_CTRL                      ((0x418c))
//Bit 31:29       reserved
//Bit 28:16       reg_fmt_ybuf_depth              // unsigned ,   RW, default = 2048
//Bit 15:12       reserved
//Bit 11: 0       reg_lbuf_depth
// unsigned ,   RW, default = 256  unit=16 pixel need to set = 2^n
#define VDIN0_A9_AFBCE_MIF_HOR_SCOPE                  ((0x418d))
//Bit 31:28       reserved
//Bit 27:16       reg_blk_end_h                   // unsigned ,   RW, default = 0
//Bit 15:12       reserved
//Bit 11: 0       reg_blk_bgn_h                   // unsigned ,   RW, default = 59
#define VDIN0_A9_AFBCE_MIF_VER_SCOPE                  ((0x418e))
//Bit 31:28       reserved
//Bit 27:16       reg_blk_end_v                   // unsigned ,   RW, default = 0
//Bit 15:12       reserved
//Bit 11: 0       reg_blk_bgn_v                   // unsigned ,   RW, default = 269
#define VDIN0_A9_AFBCE_STAT1                          ((0x418f))
//Bit 31          ro_frm_end_pulse1
// unsigned ,   RO  default = 0  frame end status
//Bit 30: 0       ro_dbg_top_info1                // unsigned ,   RO  default = 0
#define VDIN0_A9_AFBCE_STAT2                          ((0x4190))
//Bit 31: 0       ro_dbg_top_info2                // unsigned ,   RO  default = 0
#define VDIN0_A9_AFBCE_DUMMY_DATA                     ((0x4191))
//Bit 31:30       reserved
//Bit 29: 0       reg_dummy_data                  // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_CLR_FLAG                       ((0x4192))
//Bit 31: 0       reg_pls_afbce_clr_flag          // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_STA_FLAGT                      ((0x4193))
//Bit 31: 0       ro_afbce_sta_flag               // unsigned ,   RO  default = 0
#define VDIN0_A9_AFBCE_MMU_NUM                        ((0x4194))
//Bit 31:16       reserved
//Bit 15: 0       ro_frm_mmu_num                  // unsigned ,   RO  default = 0
#define VDIN0_A9_AFBCE_PIP_CTRL                       ((0x4195))
//Bit 31: 3       reserved
//Bit  2          reg_enc_align_en                // unsigned ,   RW, default = 1
//Bit  1          reg_pip_ini_ctrl                // unsigned ,   RW, default = 0
//Bit  0          reg_pip_mode                    // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_ROT_CTRL                       ((0x4196))
//Bit 31: 5       reserved
//Bit  4          reg_rot_en
// unsigned ,   RW, default = 0  rotation enable
//Bit  3: 0       reg_vstep
// unsigned ,   RW, default = 8  rotation vstep  setting acorrding rotation shrink mode
#define VDIN0_A9_AFBCE_DIMM_CTRL                      ((0x4197))
//Bit 31          reg_dimm_layer_en
// unsigned ,   RW, default = 0  dimm_layer enable signal
//Bit 30          reserved
//Bit 29: 0       reg_dimm_data
// unsigned ,   RW, default = 29'h00080200  dimm_layer data
#define VDIN0_A9_AFBCE_BND_DEC_MISC                   ((0x4198))
//Bit 31:28       reserved
//Bit 27:26       reg_bnd_dec_rev_mode
// unsigned ,   RW, default = 0  only pip mode use those bits usually don't need configure
//Bit 25:24       reg_bnd_dec_mif_urgent
// unsigned ,   RW, default = 3  only pip mode use those bits usually don't need configure
//Bit 23:22       reg_bnd_dec_burst_len
// unsigned ,   RW, default = 2  only pip mode use those bits usually don't need configure
//Bit 21:20       reg_bnd_dec_ddr_blk_size
// unsigned ,   RW, default = 1  only pip mode use those bits usually don't need configure
//Bit 19          reserved
//Bit 18:16       reg_bnd_dec_cmd_blk_size
// unsigned ,   RW, default = 3  only pip mode use those bits usually don't need configure
//Bit 15          reserved
//Bit 14          reg_bnd_dec_blk_mem_mode
// unsigned ,   RW, default = 0  only pip mode use those bits usually don't need configure
//Bit 13          reg_bnd_dec_addr_link_en
// unsigned ,   RW, default = 1  only pip mode use those bits usually don't need configure
//Bit 12          reg_bnd_dec_always_body_rden
// unsigned ,   RW, default = 0  only pip mode use those bits usually don't need configure
//Bit 11: 0       reg_bnd_dec_mif_lbuf_depth
// unsigned ,   RW, default = 128  only pip mode use those bits usually don't need configure
#define VDIN0_A9_AFBCE_RD_ARB_MISC                    ((0x4199))
//Bit 31:13       reserved
//Bit 12          reg_arb_sw_rst                  // unsigned ,   RW, default = 0
//only pip mode use those bits usually don't need configure
//Bit 11:10       reserved
//Bit  9          reg_arb_arblk_last1             // unsigned ,   RW, default = 0
//Bit  8          reg_arb_arblk_last0             // unsigned ,   RW, default = 0
//only pip mode use those bits usually don't need configure
//Bit  7: 4       reg_arb_weight_ch1              // unsigned ,   RW, default = 4
//Bit  3: 0       reg_arb_weight_ch0              // unsigned ,   RW, default = 10
#define VDIN0_A9_AFBCE_AXI_CHK_CTRL                   ((0x419a))
//Bit 31: 0       reg_axi_chk_ctrl                // unsigned ,   RW, default = 32
#define VDIN0_A9_AFBCE_AXI_CHK_STAT                   ((0x419b))
//Bit 31: 0       reg_axi_chk_stat                // unsigned ,   RO, default = 0
#define VDIN0_A9_AFBCE_ABORT_WDATA                    ((0x419c))
//Bit 31: 0       reg_abort_wdata                 // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_MMU_RMIF_CTRL1                 ((0x419d))
//Bit 31:26       reserved
//Bit 25:24       reg_sync_sel                    // unsigned ,   RW, default = 0
//Bit 23:16       reg_canvas_id                   // unsigned ,   RW, default = 0
//Bit 15          reserved
//Bit 14:12       reg_cmd_intr_len                // unsigned ,   RW, default = 1
//Bit 11:10       reg_cmd_req_size                // unsigned ,   RW, default = 1
//Bit  9: 8       reg_burst_len                   // unsigned ,   RW, default = 2
//Bit  7          reg_swap_64bit                  // unsigned ,   RW, default = 0
//Bit  6          reg_little_endian               // unsigned ,   RW, default = 1
//Bit  5          reg_y_rev                       // unsigned ,   RW, default = 0
//Bit  4          reg_x_rev                       // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_pack_mode                   // unsigned ,   RW, default = 3

#define VDIN0_A9_AFBCE_MMU_RMIF_CTRL2                 ((0x419e))
//Bit 31:30       reg_sw_rst                      // unsigned ,   RW, default = 0
//Bit 29:22       reserved
//Bit 21:20       reg_int_clr                     // unsigned ,   RW, default = 0
//Bit 19:18       reg_gclk_ctrl                   // unsigned ,   RW, default = 0
//Bit 17          reserved
//Bit 16: 0       reg_urgent_ctrl                 // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_MMU_RMIF_CTRL3                 ((0x419f))
//Bit 31:24       reserved
//Bit 23:20       reg_vstep                       // unsigned ,   RW, default = 1
//Bit 19:17       reserved
//Bit 16          reg_acc_mode                    // unsigned ,   RW, default = 1
//Bit 15:13       reserved
//Bit 12: 0       reg_stride                      // unsigned ,   RW, default = 4096
#define VDIN0_A9_AFBCE_MMU_RMIF_CTRL4                 ((0x41a0))
//Bit 31: 0       reg_baddr                       // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_MMU_RMIF_SCOPE_X               ((0x41a1))
//Bit 31:29       reserved
//Bit 28:16       reg_x_end                       // unsigned ,   RW, default = 4095
//Bit 15:13       reserved
//Bit 12: 0       reg_x_start                     // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_MMU_RMIF_SCOPE_Y               ((0x41a2))
//Bit 31:29       reserved
//Bit 28:16       reg_y_end                       // unsigned ,   RW, default = 0
//Bit 15:13       reserved
//Bit 12: 0       reg_y_start                     // unsigned ,   RW, default = 0
#define VDIN0_A9_AFBCE_MMU_RMIF_RO_STAT               ((0x41a3))
//Bit 31:16       reserved
//Bit 15: 0       reg_status                      // unsigned ,   RO, default = 0
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./vdin0_afbc_enc_regs.h
//
// 8'hb0 ~ 8'hff
//
// Reading file:  ./vdin1_afbc_enc_regs.h
//
// synopsys translate_off
// synopsys translate_on
#define VDIN1_A9_AFBCE_FORMAT                         ((0x41b0))
//Bit 31:15       reserved
//Bit 14:12       reg_burst_length_add_value      // unsigned ,   RW, default = 2
//Bit 11          reg_ofset_burst4_en             // unsigned ,   RW, default = 0
//Bit 10          reg_burst_length_add_en         // unsigned ,   RW, default = 0
//Bit  9: 8       reg_format_mode                 // unsigned ,   RW, default = 2
//Bit  7: 4       reg_compbits_c                  // unsigned ,   RW, default = 10
//Bit  3: 0       reg_compbits_y                  // unsigned ,   RW, default = 10
#define VDIN1_A9_AFBCE_MODE_EN                        ((0x41b1))
//Bit 31:28       reserved
//Bit 27:26       reserved
//Bit 25          reg_adpt_interleave_ymode       // unsigned ,   RW, default = 0
//Bit 24          reg_adpt_interleave_cmode       // unsigned ,   RW, default = 0
//Bit 23          reg_adpt_yinterleave_luma_ride  // unsigned ,   RW, default = 1
//Bit 22          reg_adpt_yinterleave_chrm_ride  // unsigned ,   RW, default = 1
//Bit 21          reg_adpt_xinterleave_luma_ride  // unsigned ,   RW, default = 1
//Bit 20          reg_adpt_xinterleave_chrm_ride  // unsigned ,   RW, default = 1
//Bit 19          reserved
//Bit 18          reg_disable_order_mode_i_6      // unsigned ,   RW, default = 0
//Bit 17          reg_disable_order_mode_i_5      // unsigned ,   RW, default = 0
//Bit 16          reg_disable_order_mode_i_4      // unsigned ,   RW, default = 0
//Bit 15          reg_disable_order_mode_i_3      // unsigned ,   RW, default = 0
//Bit 14          reg_disable_order_mode_i_2      // unsigned ,   RW, default = 0
//Bit 13          reg_disable_order_mode_i_1      // unsigned ,   RW, default = 0
//Bit 12          reg_disable_order_mode_i_0      // unsigned ,   RW, default = 0
//Bit 11          reserved
//Bit 10          reg_minval_yenc_en              // unsigned ,   RW, default = 0
//Bit  9          reg_16x4block_enable            // unsigned ,   RW, default = 0
//Bit  8          reg_uncompress_split_mode       // unsigned ,   RW, default = 0
//Bit  7: 6       reserved
//Bit  5          reg_input_padding_uv128         // unsigned ,   RW, default = 0
//Bit  4          reg_dwds_padding_uv128          // unsigned ,   RW, default = 0
//Bit  3: 1       reg_force_order_mode_value      // unsigned ,   RW, default = 0
//Bit  0          reg_force_order_mode_en         // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_DWSCALAR                       ((0x41b2))
//Bit 31: 8       reserved
//Bit  7: 6       reg_dwscalar_w0                 // unsigned ,   RW, default = 3
//Bit  5: 4       reg_dwscalar_w1                 // unsigned ,   RW, default = 0
//Bit  3: 2       reg_dwscalar_h0                 // unsigned ,   RW, default = 2
//Bit  1: 0       reg_dwscalar_h1                 // unsigned ,   RW, default = 3
#define VDIN1_A9_AFBCE_DEFCOLOR_1                     ((0x41b3))
//Bit 31:24       reserved
//Bit 23:12       reg_enc_defaultcolor_3          // unsigned ,   RW, default = 4095
//Bit 11: 0       reg_enc_defaultcolor_0          // unsigned ,   RW, default = 4095
#define VDIN1_A9_AFBCE_DEFCOLOR_2                     ((0x41b4))
//Bit 31:24       reserved
//Bit 23:12       reg_enc_defaultcolor_2          // unsigned ,   RW, default = 4095
//Bit 11: 0       reg_enc_defaultcolor_1          // unsigned ,   RW, default = 4095
#define VDIN1_A9_AFBCE_QUANT_ENABLE                   ((0x41b5))
//Bit 31:12       reserved
//Bit 11          reg_quant_expand_en_1           // unsigned ,   RW, default = 0
//Bit 10          reg_quant_expand_en_0           // unsigned ,   RW, default = 0
//Bit  9: 8       reg_bcleav_ofst                 //   signed ,   RW, default = 0
//Bit  7: 5       reserved
//Bit  4          reg_quant_enable_1              // unsigned ,   RW, default = 0
//Bit  3: 1       reserved
//Bit  0          reg_quant_enable_0              // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_IQUANT_LUT_1                   ((0x41b6))
//Bit 31          reserved
//Bit 30:28       reg_iquant_yclut_0_11           // unsigned ,   RW, default = 0
//Bit 27          reserved
//Bit 26:24       reg_iquant_yclut_0_10           // unsigned ,   RW, default = 1
//Bit 23          reserved
//Bit 22:20       reg_iquant_yclut_0_9            // unsigned ,   RW, default = 2
//Bit 19          reserved
//Bit 18:16       reg_iquant_yclut_0_8            // unsigned ,   RW, default = 3
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_0_7            // unsigned ,   RW, default = 4
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_0_6            // unsigned ,   RW, default = 5
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_0_5            // unsigned ,   RW, default = 5
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_0_4            // unsigned ,   RW, default = 4
#define VDIN1_A9_AFBCE_IQUANT_LUT_2                   ((0x41b7))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_0_3            // unsigned ,   RW, default = 3
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_0_2            // unsigned ,   RW, default = 2
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_0_1            // unsigned ,   RW, default = 1
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_0_0            // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_IQUANT_LUT_3                   ((0x41b8))
//Bit 31          reserved
//Bit 30:28       reg_iquant_yclut_1_11           // unsigned ,   RW, default = 0
//Bit 27          reserved
//Bit 26:24       reg_iquant_yclut_1_10           // unsigned ,   RW, default = 1
//Bit 23          reserved
//Bit 22:20       reg_iquant_yclut_1_9            // unsigned ,   RW, default = 2
//Bit 19          reserved
//Bit 18:16       reg_iquant_yclut_1_8            // unsigned ,   RW, default = 3
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_1_7            // unsigned ,   RW, default = 4
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_1_6            // unsigned ,   RW, default = 5
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_1_5            // unsigned ,   RW, default = 5
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_1_4            // unsigned ,   RW, default = 4
#define VDIN1_A9_AFBCE_IQUANT_LUT_4                   ((0x41b9))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_iquant_yclut_1_3            // unsigned ,   RW, default = 3
//Bit 11          reserved
//Bit 10: 8       reg_iquant_yclut_1_2            // unsigned ,   RW, default = 2
//Bit  7          reserved
//Bit  6: 4       reg_iquant_yclut_1_1            // unsigned ,   RW, default = 1
//Bit  3          reserved
//Bit  2: 0       reg_iquant_yclut_1_0            // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_RQUANT_LUT_1                   ((0x41ba))
//Bit 31          reserved
//Bit 30:28       reg_rquant_yclut_0_11           // unsigned ,   RW, default = 5
//Bit 27          reserved
//Bit 26:24       reg_rquant_yclut_0_10           // unsigned ,   RW, default = 5
//Bit 23          reserved
//Bit 22:20       reg_rquant_yclut_0_9            // unsigned ,   RW, default = 4
//Bit 19          reserved
//Bit 18:16       reg_rquant_yclut_0_8            // unsigned ,   RW, default = 4
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_0_7            // unsigned ,   RW, default = 3
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_0_6            // unsigned ,   RW, default = 3
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_0_5            // unsigned ,   RW, default = 2
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_0_4            // unsigned ,   RW, default = 2
#define VDIN1_A9_AFBCE_RQUANT_LUT_2                   ((0x41bb))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_0_3            // unsigned ,   RW, default = 1
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_0_2            // unsigned ,   RW, default = 1
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_0_1            // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_0_0            // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_RQUANT_LUT_3                   ((0x41bc))
//Bit 31          reserved
//Bit 30:28       reg_rquant_yclut_1_11           // unsigned ,   RW, default = 5
//Bit 27          reserved
//Bit 26:24       reg_rquant_yclut_1_10           // unsigned ,   RW, default = 5
//Bit 23          reserved
//Bit 22:20       reg_rquant_yclut_1_9            // unsigned ,   RW, default = 4
//Bit 19          reserved
//Bit 18:16       reg_rquant_yclut_1_8            // unsigned ,   RW, default = 4
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_1_7            // unsigned ,   RW, default = 3
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_1_6            // unsigned ,   RW, default = 3
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_1_5            // unsigned ,   RW, default = 2
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_1_4            // unsigned ,   RW, default = 2
#define VDIN1_A9_AFBCE_RQUANT_LUT_4                   ((0x41bd))
//Bit 31:16       reserved
//Bit 15          reserved
//Bit 14:12       reg_rquant_yclut_1_3            // unsigned ,   RW, default = 1
//Bit 11          reserved
//Bit 10: 8       reg_rquant_yclut_1_2            // unsigned ,   RW, default = 1
//Bit  7          reserved
//Bit  6: 4       reg_rquant_yclut_1_1            // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_rquant_yclut_1_0            // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_YUV_FORMAT_CONV_MODE           ((0x41be))
//Bit 31: 8       reserved
//Bit  7          reserved
//Bit  6: 4       reg_444to422_mode               // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_422to420_mode               // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_LOSS_CTRL                      ((0x41bf))
//Bit 31         reg_fix_cr_en                    // unsigned ,    RW, default = 1
//Bit 30         reg_rc_en                        // unsigned ,    RW, default = 0
//Bit 29         reg_write_qlevel_mode            // unsigned ,    RW, default = 1
//Bit 28         reg_debug_qlevel_en              // unsigned ,    RW, default = 0
//Bit 27:20      reserved
//Bit 19:18      reserved
//Bit 17:16      reg_cal_bit_early_mode           // unsigned ,    RW, default = 2
//Bit 15:12      reg_quant_diff_root_leave        // unsigned ,    RW, default = 2
//Bit 11: 8      reg_debug_qlevel_value           // unsigned ,    RW, default = 0
//Bit  7: 4      reg_debug_qlevel_max_0           // unsigned ,    RW, default = 10
//Bit  3: 0      reg_debug_qlevel_max_1           // unsigned ,    RW, default = 10
#define VDIN1_A9_AFBCE_LOSS_BURST_NUM                 ((0x41c0))
//Bit 31:29      reserved
//Bit 28:24      reg_block_burst_num_0            // unsigned ,    RW, default = 5
//Bit 23:21      reserved
//Bit 20:16      reg_block_burst_num_1            // unsigned ,    RW, default = 5
//Bit 15:13      reserved
//Bit 12: 8      reg_block_burst_num_2            // unsigned ,    RW, default = 5
//Bit  7: 5      reserved
//Bit  4: 0      reg_block_burst_num_3            // unsigned ,    RW, default = 5
#define VDIN1_A9_AFBCE_LOSS_RC                        ((0x41c1))
//Bit 31:29      reserved
//Bit 28         reg_rc_fifo_margin_mode          // unsigned ,    RW, default = 1
//Bit 27:24      reg_rc_1stbl_xbdgt               // unsigned ,    RW, default = 0
//Bit 23:20      reg_rc_1stln_xbdgt               // unsigned ,    RW, default = 0
//Bit 19:18      reserved
//Bit 17:16      reg_rc_qlevel_chk_mode           // unsigned ,    RW, default = 0
//Bit 15: 8      reg_fix_cr_subb_bit_adj_0        // signed ,    RW, default = 0  l
//Bit  7: 0      reg_fix_cr_subb_bit_adj_1        // signed ,    RW, default = 0  c
#define VDIN1_A9_AFBCE_LOSS_RC_FIFO_THD               ((0x41c2))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_0         // unsigned ,    RW, default = 8
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_fifo_margin_thd_1         // unsigned ,    RW, default = 16
#define VDIN1_A9_AFBCE_LOSS_RC_FIFO_BUGET             ((0x41c3))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_2         // unsigned ,    RW, default = 32
//Bit 15:12      reg_rc_fifo_margin_budget_0      // unsigned ,    RW, default = 2
//Bit 11: 8      reg_rc_fifo_margin_budget_1      // unsigned ,    RW, default = 1
//Bit  7: 4      reg_rc_fifo_margin_budget_2      // unsigned ,    RW, default = 0
//Bit  3: 0      reg_rc_max_add_budget            // unsigned ,    RW, default = 2
#define VDIN1_A9_AFBCE_LOSS_RC_ACCUM_THD_0            ((0x41c4))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_2      // unsigned ,    RW, default = 8
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_2_1      // unsigned ,    RW, default = 8
#define VDIN1_A9_AFBCE_LOSS_RC_ACCUM_THD_1            ((0x41c5))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_0      // unsigned ,    RW, default = 8
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_2      // unsigned ,    RW, default = 12
#define VDIN1_A9_AFBCE_LOSS_RC_ACCUM_THD_2            ((0x41c6))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_1_1      // unsigned ,    RW, default = 64
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_0      // unsigned ,    RW, default = 4
#define VDIN1_A9_AFBCE_LOSS_RC_ACCUM_THD_3            ((0x41c7))
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_0_2      // unsigned ,    RW, default = 25
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_0_1      // unsigned ,    RW, default = 12
#define VDIN1_A9_AFBCE_LOSS_RC_ACCUM_BUGET_0          ((0x41c8))
//Bit 31:20      reg_rc_accum_margin_thd_0_0      // unsigned ,    RW, default = 0
//Bit 19:16      reserved
//Bit 15:12      reg_rc_accum_margin_budget_2_2   // unsigned ,    RW, default = 1
//Bit 11: 8      reg_rc_accum_margin_budget_2_1   // unsigned ,    RW, default = 0
//Bit  7: 4      reg_rc_accum_margin_budget_2_0   // unsigned ,    RW, default = 0
//Bit  3: 0      reg_rc_accum_margin_budget_1_2   // unsigned ,    RW, default = 2
#define VDIN1_A9_AFBCE_LOSS_RC_ACCUM_BUGET_1          ((0x41c9))
//Bit 31:28      reg_rc_accum_margin_budget_1_1   // unsigned ,    RW, default = 1
//Bit 27:24      reg_rc_accum_margin_budget_1_0   // unsigned ,    RW, default = 1
//Bit 23:20      reg_rc_accum_margin_budget_0_2   // unsigned ,    RW, default = 2
//Bit 19:16      reg_rc_accum_margin_budget_0_1   // unsigned ,    RW, default = 1
//Bit 15:12      reg_rc_accum_margin_budget_0_0   // unsigned ,    RW, default = 1
//Bit 11: 8      reserved
//Bit  7: 4      reg_rc_qlevel_chk_th_0           // unsigned ,    RW, default = 5
//Bit  3: 0      reg_rc_qlevel_chk_th_1           // unsigned ,    RW, default = 4
#define VDIN1_A9_AFBCE_LOSS_RO_ERROR_L_0              ((0x41ca))
//Bit 31: 0      ro_error_acc_mode0_l_0           // unsigned ,    RO, default = 0  square
#define VDIN1_A9_AFBCE_LOSS_RO_COUNT_0                ((0x41cb))
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_0             // unsigned ,    RO, default = 0 mode num contour
#define VDIN1_A9_AFBCE_LOSS_RO_ERROR_L_1              ((0x41cc))
//Bit 31: 0      ro_error_acc_mode0_l_1           // unsigned ,    RO, default = 0  square
#define VDIN1_A9_AFBCE_LOSS_RO_COUNT_1                ((0x41cd))
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_1             // unsigned ,    RO, default = 0 mode num contour
#define VDIN1_A9_AFBCE_LOSS_RO_ERROR_L_2              ((0x41ce))
//Bit 31: 0      ro_error_acc_mode0_l_2           // unsigned ,    RO, default = 0  square
#define VDIN1_A9_AFBCE_LOSS_RO_COUNT_2                ((0x41cf))
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_2             // unsigned ,    RO, default = 0 mode num contour
#define VDIN1_A9_AFBCE_LOSS_RO_ERROR_H_0              ((0x41d0))
//Bit 31:16      ro_error_acc_mode0_h_2           // unsigned ,    RO, default = 0  square
//Bit 15: 0      ro_error_acc_mode0_h_1           // unsigned ,    RO, default = 0  square
#define VDIN1_A9_AFBCE_LOSS_RO_ERROR_H_1              ((0x41d1))
//Bit 31:16      reserved
//Bit 15: 0      ro_error_acc_mode0_h_0           // unsigned ,    RO, default = 0  square
#define VDIN1_A9_AFBCE_LOSS_RO_MAX_ERROR_0            ((0x41d2))
//Bit 31:28      reserved
//Bit 27:16      ro_max_error_mode0_2             // unsigned ,    RO, default = 0  error
//Bit 15:12      reserved
//Bit 11: 0      ro_max_error_mode0_1             // unsigned ,    RO, default = 0  error
#define VDIN1_A9_AFBCE_LOSS_RO_MAX_ERROR_1            ((0x41d3))
//Bit 31:12      reserveds
//Bit 11: 0      ro_max_error_mode0_0             // unsigned ,    RO, default = 0  error
#define VDIN1_A9_AFBCE_ENABLE                         ((0x41d4))
//Bit 31:20       reg_gclk_ctrl                   // unsigned ,   RW, default = 0
//Bit 19:16       reg_afbce_sync_sel              // unsigned ,   RW, default = 0
//Bit 15:14       reserved
//Bit 13          reg_enc_rst_mode                // unsigned ,   RW, default = 0
//Bit 12          reg_enc_en_mode                 // unsigned ,   RW, default = 0
//Bit 11: 9       reserved
//Bit  8          reg_enc_enable                  // unsigned ,   RW, default = 0
//Bit  7: 1       reserved
//Bit  0          reg_pls_enc_frm_start           // unsigned ,   W1C, default = 0
#define VDIN1_A9_AFBCE_MODE                           ((0x41d5))
//Bit 31:29       reg_soft_rst                    // unsigned ,   RW, default = 0
//Bit 28          reg_axi_chk_sel                 // unsigned ,   RW, default = 0
//Bit 27:26       reg_rev_mode                    // unsigned ,   RW, default = 0
//Bit 25:24       reg_mif_urgent                  // unsigned ,   RW, default = 3
//Bit 23          reg_abort_wstrb                 // unsigned ,   RW, default = 0
//Bit 22:16       reg_hold_line_num               // unsigned ,   RW, default = 4
//Bit 15:14       reg_burst_mode                  // unsigned ,   RW, default = 2
//Bit 13: 1       reserved
//Bit  0          reg_fmt444_comb
// unsigned ,   RW, default = 0  0: 444 8bit comb mode
#define VDIN1_A9_AFBCE_SIZE_IN                        ((0x41d6))
//Bit 31:16       reg_hsize_bg                    // unsigned ,   RW, default = 1920
//Bit 15: 0       reg_vsize_bg                    // unsigned ,   RW, default = 1080
#define VDIN1_A9_AFBCE_BLK_SIZE_IN                    ((0x41d7))
//Bit 31:16       reg_hblk_size                   // unsigned ,   RW, default = 60
//Bit 15: 0       reg_vblk_size                   // unsigned ,   RW, default = 270
#define VDIN1_A9_AFBCE_HEAD_BADDR                     ((0x41d8))
//Bit 31: 0       reg_head_baddr                  // unsigned ,   RW, default = 32'h00 head address
#define VDIN1_A9_AFBCE_MIF_SIZE                       ((0x41d9))
//Bit 31:30       reserved
//Bit 29:28       reg_ddr_blk_size                // unsigned ,   RW, default = 1
//Bit 27          reserved
//Bit 26:24       reg_cmd_blk_size                // unsigned ,   RW, default = 3
//Bit 23:16       reg_uncmp_size                  // unsigned ,   RW, default = 20
//Bit 15: 0       reg_mmu_page_size               // unsigned ,   RW, default = 4096
#define VDIN1_A9_AFBCE_PIXEL_IN_HOR_SCOPE             ((0x41da))
//Bit 31:16       reg_enc_win_end_h               // unsigned ,   RW, default = 1919
//Bit 15: 0       reg_enc_win_bgn_h               // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_PIXEL_IN_VER_SCOPE             ((0x41db))
//Bit 31:16       reg_enc_win_end_v               // unsigned ,   RW, default = 1079
//Bit 15: 0       reg_enc_win_bgn_v               // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_CONV_CTRL                      ((0x41dc))
//Bit 31:29       reserved
//Bit 28:16       reg_fmt_ybuf_depth              // unsigned ,   RW, default = 2048
//Bit 15:12       reserved
//Bit 11: 0       reg_lbuf_depth                  // unsigned ,   RW, default = 256
#define VDIN1_A9_AFBCE_MIF_HOR_SCOPE                  ((0x41dd))
//Bit 31:28       reserved
//Bit 27:16       reg_blk_end_h                   // unsigned ,   RW, default = 0
//Bit 15:12       reserved
//Bit 11: 0       reg_blk_bgn_h                   // unsigned ,   RW, default = 59
#define VDIN1_A9_AFBCE_MIF_VER_SCOPE                  ((0x41de))
//Bit 31:28       reserved
//Bit 27:16       reg_blk_end_v                   // unsigned ,   RW, default = 0
//Bit 15:12       reserved
//Bit 11: 0       reg_blk_bgn_v                   // unsigned ,   RW, default = 269
#define VDIN1_A9_AFBCE_STAT1                          ((0x41df))
//Bit 31          ro_frm_end_pulse1               // unsigned ,   RO  default = 0  frame end status
//Bit 30: 0       ro_dbg_top_info1                // unsigned ,   RO  default = 0
#define VDIN1_A9_AFBCE_STAT2                          ((0x41e0))
//Bit 31: 0       ro_dbg_top_info2                // unsigned ,   RO  default = 0
#define VDIN1_A9_AFBCE_DUMMY_DATA                     ((0x41e1))
//Bit 31:30       reserved
//Bit 29: 0       reg_dummy_data                  // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_CLR_FLAG                       ((0x41e2))
//Bit 31: 0       reg_pls_afbce_clr_flag          // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_STA_FLAGT                      ((0x41e3))
//Bit 31: 0       ro_afbce_sta_flag               // unsigned ,   RO  default = 0
#define VDIN1_A9_AFBCE_MMU_NUM                        ((0x41e4))
//Bit 31:16       reserved
//Bit 15: 0       ro_frm_mmu_num                  // unsigned ,   RO  default = 0
#define VDIN1_A9_AFBCE_PIP_CTRL                       ((0x41e5))
//Bit 31: 3       reserved
//Bit  2          reg_enc_align_en                // unsigned ,   RW, default = 1
//Bit  1          reg_pip_ini_ctrl                // unsigned ,   RW, default = 0
//Bit  0          reg_pip_mode                    // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_ROT_CTRL                       ((0x41e6))
//Bit 31: 5       reserved
//Bit  4          reg_rot_en                      // unsigned ,   RW, default = 0
//Bit  3: 0       reg_vstep                       // unsigned ,   RW, default = 8
#define VDIN1_A9_AFBCE_DIMM_CTRL                      ((0x41e7))
//Bit 31          reg_dimm_layer_en               // unsigned ,   RW, default = 0
//Bit 30          reserved
//Bit 29: 0       reg_dimm_data                   // unsigned ,   RW, default = 29'h00080200
#define VDIN1_A9_AFBCE_BND_DEC_MISC                   ((0x41e8))
//Bit 31:28       reserved
//Bit 27:26       reg_bnd_dec_rev_mode            // unsigned ,   RW, default = 0
//Bit 25:24       reg_bnd_dec_mif_urgent          // unsigned ,   RW, default = 3
//Bit 23:22       reg_bnd_dec_burst_len           // unsigned ,   RW, default = 2
//Bit 21:20       reg_bnd_dec_ddr_blk_size        // unsigned ,   RW, default = 1
//Bit 19          reserved
//Bit 18:16       reg_bnd_dec_cmd_blk_size        // unsigned ,   RW, default = 3
//Bit 15          reserved
//Bit 14          reg_bnd_dec_blk_mem_mode        // unsigned ,   RW, default = 0
//Bit 13          reg_bnd_dec_addr_link_en        // unsigned ,   RW, default = 1
//Bit 12          reg_bnd_dec_always_body_rden    // unsigned ,   RW, default = 0
//Bit 11: 0       reg_bnd_dec_mif_lbuf_depth      // unsigned ,   RW, default = 12
#define VDIN1_A9_AFBCE_RD_ARB_MISC                    ((0x41e9))
//Bit 31:13       reserved
//Bit 12          reg_arb_sw_rst                  // unsigned ,   RW, default = 0
//Bit 11:10       reserved
//Bit  9          reg_arb_arblk_last1             // unsigned ,   RW, default = 0
//Bit  8          reg_arb_arblk_last0             // unsigned ,   RW, default = 0
//Bit  7: 4       reg_arb_weight_ch1              // unsigned ,   RW, default = 4
//Bit  3: 0       reg_arb_weight_ch0              // unsigned ,   RW, default = 10
#define VDIN1_A9_AFBCE_AXI_CHK_CTRL                   ((0x41ea))
//Bit 31: 0       reg_axi_chk_ctrl                // unsigned ,   RW, default = 32'h6426725
#define VDIN1_A9_AFBCE_AXI_CHK_STAT                   ((0x41eb))
//Bit 31: 0       reg_axi_chk_stat                // unsigned ,   RO, default = 0
#define VDIN1_A9_AFBCE_ABORT_WDATA                    ((0x41ec))
//Bit 31: 0       reg_abort_wdata                 // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_MMU_RMIF_CTRL1                 ((0x41ed))
//Bit 31:26       reserved
//Bit 25:24       reg_sync_sel                    // unsigned ,   RW, default = 0
//Bit 23:16       reg_canvas_id                   // unsigned ,   RW, default = 0
//Bit 15          reserved
//Bit 14:12       reg_cmd_intr_len                // unsigned ,   RW, default = 1
//Bit 11:10       reg_cmd_req_size                // unsigned ,   RW, default = 1
//Bit  9: 8       reg_burst_len                   // unsigned ,   RW, default = 2
//Bit  7          reg_swap_64bit                  // unsigned ,   RW, default = 0
//Bit  6          reg_little_endian               // unsigned ,   RW, default = 1
//Bit  5          reg_y_rev                       // unsigned ,   RW, default = 0
//Bit  4          reg_x_rev                       // unsigned ,   RW, default = 0
//Bit  3          reserved
//Bit  2: 0       reg_pack_mode                   // unsigned ,   RW, default = 3
#define VDIN1_A9_AFBCE_MMU_RMIF_CTRL2                 ((0x41ee))
//Bit 31:30       reg_sw_rst                      // unsigned ,   RW, default = 0
//Bit 29:22       reserved
//Bit 21:20       reg_int_clr                     // unsigned ,   RW, default = 0
//Bit 19:18       reg_gclk_ctrl                   // unsigned ,   RW, default = 0
//Bit 17          reserved
//Bit 16: 0       reg_urgent_ctrl                 // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_MMU_RMIF_CTRL3                 ((0x41ef))
//Bit 31:24       reserved
//Bit 23:20       reg_vstep                       // unsigned ,   RW, default = 1
//Bit 19:17       reserved
//Bit 16          reg_acc_mode                    // unsigned ,   RW, default = 1
//Bit 15:13       reserved
//Bit 12: 0       reg_stride                      // unsigned ,   RW, default = 4096
#define VDIN1_A9_AFBCE_MMU_RMIF_CTRL4                 ((0x41f0))
//Bit 31: 0       reg_baddr                       // unsigned ,   RW, default = 0
#define VDIN1_A9_AFBCE_MMU_RMIF_SCOPE_X               ((0x41f1))
//Bit 31:29       reserved
//Bit 28:16       reg_x_end
// unsigned ,   RW, default = 4095  the canvas hor end pixel position
//Bit 15:13       reserved
//Bit 12: 0       reg_x_start
// unsigned ,   RW, default = 0  the canvas hor start pixel position
#define VDIN1_A9_AFBCE_MMU_RMIF_SCOPE_Y               ((0x41f2))
//Bit 31:29       reserved
//Bit 28:16       reg_y_end
// unsigned ,   RW, default = 0  the canvas ver end pixel position
//Bit 15:13       reserved
//Bit 12: 0       reg_y_start
// unsigned ,   RW, default = 0  the canvas ver start pixel position
#define VDIN1_A9_AFBCE_MMU_RMIF_RO_STAT               ((0x41f3))
//Bit 31:16       reserved
//Bit 15: 0       reg_status
// unsigned ,   RO, default = 0

#define VDIN1_A9_HDR2_CTRL                            ((0x2620  << 2) + 0xff500000)
//Bit 31:25        reserved
//Bit 24           reg_ergb_sel_mode
//unsigned ,    RW, default = 0 hist input source select, 0: input rgb 1: output rgb
//Bit 23:21        reserved
//Bit 20:18        reg_din_swap              // unsigned ,    RW, default = 0
//Bit 17           reg_out_rgb               // unsigned ,    RW, default = 1
//Bit 16           reg_only_mat              // unsigned ,    RW, default = 0
//Bit 15           reg_mtrxo_en              // unsigned ,    RW, default = 1
//Bit 14           reg_mtrxi_en              // unsigned ,    RW, default = 1
//Bit 13           reg_hdr2_top_en           // unsigned ,    RW, default = 0
//Bit 12           reg_c_gain_mode           // unsigned ,    RW, default = 1
//Bit 11: 8        reserved
//Bit  7: 6        reg_gmut_mode             // unsigned ,    RW, default = 1
//Bit  5           reg_in_shift
// unsigned ,    RW, default = 0  0: use input U10 process,IE_BW=10; 1: use input u12 process
//Bit  4           reg_in_fmt
// unsigned ,    RW, default = 1  20180719,input already RGB(2020),no need trans
//Bit  3           reg_eo_enable             // unsigned ,    RW, default = 1
//Bit  2           reg_oe_enable             // unsigned ,    RW, default = 1
//Bit  1           reg_ogain_enable          // unsigned ,    RW, default = 1
//Bit  0           reg_cgain_enable          // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_CLK_GATE                        ((0x2621  << 2) + 0xff500000)
//Bit 31: 0        reg_gclk_ctrl             // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_MATRIXI_COEF00_01               ((0x2622  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxi_coef00        // signed ,    RW, default = 1023  reg_mtrxi_coef_00
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxi_coef01        // signed ,    RW, default = 0  reg_mtrxi_coef_01
#define VDIN1_A9_HDR2_MATRIXI_COEF02_10               ((0x2623  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxi_coef02        // signed ,    RW, default = 1510  reg_mtrxi_coef_02
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxi_coef10        // signed ,    RW, default = 1023  reg_mtrxi_coef_10
#define VDIN1_A9_HDR2_MATRIXI_COEF11_12               ((0x2624  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxi_coef11        // signed ,    RW, default = -168  reg_mtrxi_coef_11
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxi_coef12        // signed ,    RW, default = -585  reg_mtrxi_coef_12
#define VDIN1_A9_HDR2_MATRIXI_COEF20_21               ((0x2625  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxi_coef20        // signed ,    RW, default = 1023  reg_mtrxi_coef_20
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxi_coef21        // signed ,    RW, default = 1926  reg_mtrxi_coef_21
#define VDIN1_A9_HDR2_MATRIXI_COEF22                  ((0x2626  << 2) + 0xff500000)
//Bit 31:13        reserved
//Bit 12: 0        reg_mtrxi_coef22        // signed ,    RW, default = 0  reg_mtrxi_coef_22
#define VDIN1_A9_HDR2_MATRIXI_COEF30_31               ((0x2627  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxi_coef30        // signed ,    RW, default = 0  reg_mtrxi_coef_30
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxi_coef31        // signed ,    RW, default = 0  reg_mtrxi_coef_31
#define VDIN1_A9_HDR2_MATRIXI_COEF32_40               ((0x2628  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxi_coef32        // signed ,    RW, default = 0  reg_mtrxi_coef_32
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxi_coef40        // signed ,    RW, default = 0  reg_mtrxi_coef_40
#define VDIN1_A9_HDR2_MATRIXI_COEF41_42               ((0x2629  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxi_coef41        // signed ,    RW, default = 0  reg_mtrxi_coef_41
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxi_coef42        // signed ,    RW, default = 0  reg_mtrxi_coef_42
#define VDIN1_A9_HDR2_MATRIXI_OFFSET0_1               ((0x262a  << 2) + 0xff500000)
//Bit 31:27        reserved
//Bit 26:16        reg_mtrxi_offst_oup0     // signed ,    RW, default = 0  reg_mtrxi_offst_oup_0
//Bit 15:11        reserved
//Bit 10: 0        reg_mtrxi_offst_oup1     // signed ,    RW, default = 0  reg_mtrxi_offst_oup_1
#define VDIN1_A9_HDR2_MATRIXI_OFFSET2                 ((0x262b  << 2) + 0xff500000)
//Bit 31:11        reserved
//Bit 10: 0        reg_mtrxi_offst_oup2     // signed ,    RW, default = 0  reg_mtrxi_offst_oup_2
#define VDIN1_A9_HDR2_MATRIXI_PRE_OFFSET0_1           ((0x262c  << 2) + 0xff500000)
//Bit 31:27        reserved
//Bit 26:16        reg_mtrxi_offst_inp0     // signed ,    RW, default = 0
//Bit 15:11        reserved
//Bit 10: 0        reg_mtrxi_offst_inp1     // signed ,    RW, default = -512
#define VDIN1_A9_HDR2_MATRIXI_PRE_OFFSET2             ((0x262d  << 2) + 0xff500000)
//Bit 31:11        reserved
//Bit 10: 0        reg_mtrxi_offst_inp2     // signed ,    RW, default = -512
#define VDIN1_A9_HDR2_MATRIXO_COEF00_01               ((0x262e  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef00        // signed ,    RW, default = 218
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef01        // signed ,    RW, default = 732
#define VDIN1_A9_HDR2_MATRIXO_COEF02_10               ((0x262f  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef02        // signed ,    RW, default = 74
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef10        // signed ,    RW, default = -117
#define VDIN1_A9_HDR2_MATRIXO_COEF11_12               ((0x2630  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef11        // signed ,    RW, default = -395
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef12        // signed ,    RW, default = 512
#define VDIN1_A9_HDR2_MATRIXO_COEF20_21               ((0x2631  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef20        // signed ,    RW, default = 512
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef21        // signed ,    RW, default = -465
#define VDIN1_A9_HDR2_MATRIXO_COEF22                  ((0x2632  << 2) + 0xff500000)
//Bit 31:13        reserved
//Bit 12: 0        reg_mtrxo_coef22        // signed ,    RW, default = -4
#define VDIN1_A9_HDR2_MATRIXO_COEF30_31               ((0x2633  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef30        // signed ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef31        // signed ,    RW, default = 0
#define VDIN1_A9_HDR2_MATRIXO_COEF32_40               ((0x2634  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef32        // signed ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef40        // signed ,    RW, default = 0
#define VDIN1_A9_HDR2_MATRIXO_COEF41_42               ((0x2635  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef41        // signed ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef42        // signed ,    RW, default = 0
#define VDIN1_A9_HDR2_MATRIXO_OFFSET0_1               ((0x2636  << 2) + 0xff500000)
//Bit 31:27        reserved
//Bit 26:16        reg_mtrxo_offst_oup0     // signed ,    RW, default = 0  reg_mtrxo_offst_oup_0
//Bit 15:11        reserved
//Bit 10: 0        reg_mtrxo_offst_oup1     // signed ,    RW, default = 512  reg_mtrxo_offst_oup_1
#define VDIN1_A9_HDR2_MATRIXO_OFFSET2                 ((0x2637  << 2) + 0xff500000)
//Bit 31:11        reserved
//Bit 10: 0        reg_mtrxo_offst_oup2     // signed ,    RW, default = 512  reg_mtrxo_offst_oup_2
#define VDIN1_A9_HDR2_MATRIXO_PRE_OFFSET0_1           ((0x2638  << 2) + 0xff500000)
//Bit 31:27        reserved
//Bit 26:16        reg_mtrxo_offst_inp0     // signed ,    RW, default = 0  reg_mtrxo_offst_inp_0
//Bit 15:11        reserved
//Bit 10: 0        reg_mtrxo_offst_inp1     // signed ,    RW, default = 0  reg_mtrxo_offst_inp_1
#define VDIN1_A9_HDR2_MATRIXO_PRE_OFFSET2             ((0x2639  << 2) + 0xff500000)
//Bit 31:11        reserved
//Bit 10: 0        reg_mtrxo_offst_inp2     // signed ,    RW, default = 0  reg_mtrxo_offst_inp_2
#define VDIN1_A9_HDR2_MATRIXI_CLIP                    ((0x263a  << 2) + 0xff500000)
//Bit 31:20        reserved
//Bit 19: 8        reg_mtrxi_comp_thrd       // unsigned ,    RW, default = 0
//Bit  7: 5        reg_mtrxi_rs              // signed ,    RW, default = 0
//Bit  4: 3        reg_mtrxi_clmod           // unsigned ,    RW, default = 0
//Bit  2: 0        reserved
#define VDIN1_A9_HDR2_MATRIXO_CLIP                    ((0x263b  << 2) + 0xff500000)
//Bit 31:20        reserved
//Bit 19: 8        reg_mtrxo_comp_thrd       // unsigned ,    RW, default = 0
//Bit  7: 5        reg_mtrxo_rs              // signed ,    RW, default = 0
//Bit  4: 3        reg_mtrxo_clmod           // unsigned ,    RW, default = 0
//Bit  2: 0        reserved
#define VDIN1_A9_HDR2_CGAIN_OFFT                      ((0x263c  << 2) + 0xff500000)
//Bit 31:27        reserved
//Bit 26:16        reg_cgain_oft2            // unsigned ,    RW, default = 512  hw reg
//Bit 15:11        reserved
//Bit 10: 0        reg_cgain_oft1            // unsigned ,    RW, default = 512  hw reg
#define VDIN1_A9_HDR2_HIST_RD                         ((0x263d  << 2) + 0xff500000)
//Bit 31:24        reserved
//Bit 23: 0        reg_hist_status           // unsigned ,    RW, default = 0
#define VDIN1_A9_EOTF_LUT_ADDR_PORT                   ((0x263e  << 2) + 0xff500000)
#define VDIN1_A9_EOTF_LUT_DATA_PORT                   ((0x263f  << 2) + 0xff500000)
#define VDIN1_A9_OETF_LUT_ADDR_PORT                   ((0x2640  << 2) + 0xff500000)
#define VDIN1_A9_OETF_LUT_DATA_PORT                   ((0x2641  << 2) + 0xff500000)
#define VDIN1_A9_CGAIN_LUT_ADDR_PORT                  ((0x2642  << 2) + 0xff500000)
#define VDIN1_A9_CGAIN_LUT_DATA_PORT                  ((0x2643  << 2) + 0xff500000)
#define VDIN1_A9_HDR2_CGAIN_COEF0                     ((0x2644  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27:16        reg_coef1                 // unsigned ,    RW, default = 2376
//Bit 15:12        reserved
//Bit 11: 0        reg_coef0                 // unsigned ,    RW, default = 920
#define VDIN1_A9_HDR2_CGAIN_COEF1                     ((0x2645  << 2) + 0xff500000)
//Bit 31           reg_sel_opt               // unsigned ,    RW, default = 1
//Bit 30:28        reserved
//Bit 27:16        reg_maxrgb                // unsigned ,    RW, default = 1023
//Bit 15:12        reserved
//Bit 11: 0        reg_c_gain_lim_coef_2     // unsigned ,    RW, default = 208
#define VDIN1_A9_OGAIN_LUT_ADDR_PORT                  ((0x2646  << 2) + 0xff500000)
#define VDIN1_A9_OGAIN_LUT_DATA_PORT                  ((0x2647  << 2) + 0xff500000)
#define VDIN1_A9_HDR2_ADPS_CTRL                       ((0x2648  << 2) + 0xff500000)
//Bit 31:24        reserved
//Bit 23:20        reg_adpscl1_sft           // unsigned ,    RW, default = 5
//Bit 19:18        reserved
//Bit 17           reg_ogain_blend_mode      // unsigned ,    RW, default = 0
//Bit 16           reg_adpscl_sel_opt        // unsigned ,    RW, default = 1
//Bit 15:14        reserved
//Bit 13: 8        reg_adpscl_max            // unsigned ,    RW, default = 24
//Bit  7           reg_adpscl_clip_en        // unsigned ,    RW, default = 0
//Bit  6           reg_adpscl_bypass2        // unsigned ,    RW, default = 0
//Bit  5           reg_adpscl_bypass1        // unsigned ,    RW, default = 0
//Bit  4           reg_adpscl_bypass0        // unsigned ,    RW, default = 0
//Bit  3: 2        reg_adpscl1_mode          // unsigned ,    RW, default = 1
//Bit  1: 0        reg_adpscl_mode           // unsigned ,    RW, default = 1
#define VDIN1_A9_HDR2_ADPS_ALPHA0                     ((0x2649  << 2) + 0xff500000)
//Bit 31:30        reserved
//Bit 29:16        reg_adpscl_alpha1        // unsigned ,    RW, default = 1024
//Bit 15:14        reserved
//Bit 13: 0        reg_adpscl_alpha0        // unsigned ,    RW, default = 1024
#define VDIN1_A9_HDR2_ADPS_ALPHA1                     ((0x264a  << 2) + 0xff500000)
//Bit 31:28        reg_adpscl_shift0         // unsigned ,    RW, default = 12
//Bit 27:25        reserved
//Bit 24:20        reg_adpscl_shift1         // unsigned ,    RW, default = 14
//Bit 19:16        reg_adpscl_shift2         // unsigned ,    RW, default = 12  hw no use
//Bit 15:14        reserved
//Bit 13: 0        reg_adpscl_alpha2        // unsigned ,    RW, default = 1024
#define VDIN1_A9_HDR2_ADPS_BETA0                      ((0x264b  << 2) + 0xff500000)
//Bit 31:21        reserved
//Bit 20: 0        reg_adpscl_beta0         // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_ADPS_BETA1                      ((0x264c  << 2) + 0xff500000)
//Bit 31:21        reserved
//Bit 20: 0        reg_adpscl_beta1         // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_ADPS_BETA2                      ((0x264d  << 2) + 0xff500000)
//Bit 31:21        reserved
//Bit 20: 0        reg_adpscl_beta2         // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_ADPS_COEF0                      ((0x264e  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27:16        reg_adpscl_ys_coef1      // unsigned ,    RW, default = 1024
//Bit 15:12        reserved
//Bit 11: 0        reg_adpscl_ys_coef0      // unsigned ,    RW, default = 1024
#define VDIN1_A9_HDR2_ADPS_COEF1                      ((0x264f  << 2) + 0xff500000)
//Bit 31:12        reserved
//Bit 11: 0        reg_adpscl_ys_coef2      // unsigned ,    RW, default = 1024
#define VDIN1_A9_HDR2_GMUT_CTRL                       ((0x2650  << 2) + 0xff500000)
//Bit 31: 5        reserved
//Bit  4           reg_new_mode              // unsigned ,    RW, default = 0
//Bit  3: 0        reg_gmut_shift            // unsigned ,    RW, default = 8
#define VDIN1_A9_HDR2_GMUT_COEF0                      ((0x2651  << 2) + 0xff500000)
//Bit 31:16        reg_gmut_coef01         // signed ,    RW, default = -150
//Bit 15: 0        reg_gmut_coef00         // signed ,    RW, default = 425
#define VDIN1_A9_HDR2_GMUT_COEF1                      ((0x2652  << 2) + 0xff500000)
//Bit 31:16        reg_gmut_coef10         // signed ,    RW, default = -31
//Bit 15: 0        reg_gmut_coef02         // signed ,    RW, default = -18
#define VDIN1_A9_HDR2_GMUT_COEF2                      ((0x2653  << 2) + 0xff500000)
//Bit 31:16        reg_gmut_coef12         // signed ,    RW, default = -2
//Bit 15: 0        reg_gmut_coef11         // signed ,    RW, default = 290
#define VDIN1_A9_HDR2_GMUT_COEF3                      ((0x2654  << 2) + 0xff500000)
//Bit 31:16        reg_gmut_coef21         // signed ,    RW, default = -25
//Bit 15: 0        reg_gmut_coef20         // signed ,    RW, default = -5
#define VDIN1_A9_HDR2_GMUT_COEF4                      ((0x2655  << 2) + 0xff500000)
//Bit 31:16        reserved
//Bit 15: 0        reg_gmut_coef_2_2         // signed ,    RW, default = 286
#define VDIN1_A9_HDR2_PIPE_CTRL1                      ((0x2656  << 2) + 0xff500000)
//Bit 31:24        reg_vblank_num_oetf       // unsigned ,    RW, default = 4
//Bit 23:16        reg_hblank_num_oetf       // unsigned ,    RW, default = 4
//Bit 15: 8        reg_vblank_num_eotf       // unsigned ,    RW, default = 10
//Bit  7: 0        reg_hblank_num_eotf       // unsigned ,    RW, default = 10
#define VDIN1_A9_HDR2_PIPE_CTRL2                      ((0x2657  << 2) + 0xff500000)
//Bit 31:24        reg_vblank_num_cgain      // unsigned ,    RW, default = 10
//Bit 23:16        reg_hblank_num_cgain      // unsigned ,    RW, default = 10
//Bit 15: 8        reg_vblank_num_gmut       // unsigned ,    RW, default = 17
//Bit  7: 0        reg_hblank_num_gmut       // unsigned ,    RW, default = 17
#define VDIN1_A9_HDR2_PIPE_CTRL3                      ((0x2658  << 2) + 0xff500000)
//Bit 31:24        reg_vblank_num_adps       // unsigned ,    RW, default = 22
//Bit 23:16        reg_hblank_num_adps       // unsigned ,    RW, default = 22
//Bit 15: 8        reg_vblank_num_uv         // unsigned ,    RW, default = 4
//Bit  7: 0        reg_hblank_num_uv         // unsigned ,    RW, default = 4
#define VDIN1_A9_HDR2_PROC_WIN1                       ((0x2659  << 2) + 0xff500000)
//Bit 31           reg_proc_win_gmut_en      // unsigned ,    RW, default = 0  hw reg
//Bit 30           reg_proc_win_adps_en      // unsigned ,    RW, default = 0  hw reg
//Bit 29           reg_proc_win_cgain_en     // unsigned ,    RW, default = 0  hw reg
//Bit 28:16        reg_proc_x_ed             // unsigned ,    RW, default = 9
//Bit 15:13        reserved
//Bit 12: 0        reg_proc_x_st             // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_PROC_WIN2                       ((0x265a  << 2) + 0xff500000)
//Bit 31:30        reserved
//Bit 29           reg_proc_win_aicr_en      // unsigned ,    RW, default = 1  hw reg
//Bit 28:16        reg_proc_y_ed             // unsigned ,    RW, default = 99
//Bit 15:13        reserved
//Bit 12: 0        reg_proc_y_st             // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_MATRIXI_EN_CTRL                 ((0x265b  << 2) + 0xff500000)
//Bit 31: 8        reserved
//Bit  7: 0        reg_matrixi_en_ctrl       // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_MATRIXO_EN_CTRL                 ((0x265c  << 2) + 0xff500000)
//Bit 31: 8        reserved
//Bit  7: 0        reg_matrixo_en_ctrl       // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_HIST_CTRL                       ((0x265d  << 2) + 0xff500000)
//Bit 31:25        reserved
//Bit 24:17        reg_vcbus_rd_idx          // unsigned ,    RW, default = 0  hw reg
//Bit 16           reg_hist_enable           // unsigned ,    RW, default = 0  hw reg
//Bit 15: 8        reg_gclk_ctrl0            // unsigned ,    RW, default = 20  hw reg
//Bit  7: 6        reserved
//Bit  5           reg_piecewise_mode        // unsigned ,    RW, default = 0
//Bit  4           reg_hist_win_mode         // unsigned ,    RW, default = 1
//Bit  3           reg_maxRGB_rshift         // unsigned ,    RW, default = 0
//Bit  2: 0        reg_maxRGB_sel            // unsigned ,    RW, default = 0
#define VDIN1_A9_HDR2_HIST_H_START_END                ((0x265e  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_hist_proc_x_st        // unsigned ,    RW, default = 0  hist proc start x
//Bit 15:13        reserved
//Bit 12: 0        reg_hist_proc_x_ed        // unsigned ,    RW, default = 99  hist proc end x
#define VDIN1_A9_HDR2_HIST_V_START_END                ((0x265f  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_hist_proc_y_st        // unsigned ,    RW, default = 0  hist proc start y
//Bit 15:13        reserved
//Bit 12: 0        reg_hist_proc_y_ed        // unsigned ,    RW, default = 99  hist proc end y
#define VDIN1_A9_OGAIN_LUT1_ADDR_PORT                 ((0x2660  << 2) + 0xff500000)
#define VDIN1_A9_OGAIN_LUT1_DATA_PORT                 ((0x2661  << 2) + 0xff500000)
#define VDIN1_A9_HDR2_CGAIN_VIVID                     ((0x2662  << 2) + 0xff500000)
//Bit 31:30        reserved
//Bit 29:26        reg_omax_sync_gain_sft    // unsigned ,    RW, default = 0
//Bit 25:16        reg_omax_sync_gain        // unsigned ,    RW, default = 1
//Bit 15: 4        reserved
//Bit 3:2          reg_cgain_pos             // unsigned ,    RW, default = 0
//Bit 1            reg_ogain_inser           // unsigned ,    RW, default = 0
//Bit 0            reserved
#define VDIN1_A9_HDR2_GMUT_COMP0                      ((0x2663  << 2) + 0xff500000)
//Bit 31           reg_hdr2_gm_comp_en       // unsigned ,    RW, default = 1
//Bit 30:28        reserved
//Bit 27: 8        reg_hdr_comp_ofst_r       // unsigned ,    RW, default = 85900
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP1                      ((0x2664  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_ofst_g       // unsigned ,    RW, default = 85900
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP2                      ((0x2665  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_ofst_b       // unsigned ,    RW, default = 85900
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP3                      ((0x2666  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_min_r        // unsigned ,    RW, default = 510025
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP4                      ((0x2667  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_min_g        // unsigned ,    RW, default = 472965
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP5                      ((0x2668  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_min_b        // unsigned ,    RW, default = 467019
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP6                      ((0x2669  << 2) + 0xff500000)
//Bit 31:30        reserved
//Bit 29: 8        reg_hdr_comp_rat_r        // unsigned ,    RW, default = 15210
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP7                      ((0x266a  << 2) + 0xff500000)
//Bit 31:30        reserved
//Bit 29: 8        reg_hdr_comp_rat_g        // unsigned ,    RW, default = 472965
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_GMUT_COMP8                      ((0x266b  << 2) + 0xff500000)
//Bit 31:30        reserved
//Bit 29: 8        reg_hdr_comp_rat_b        // unsigned ,    RW, default = 467019
//Bit  7: 0        reserved
#define VDIN1_A9_HDR2_HIST_SLC_X_ST_ED_0              ((0x266c  << 2) + 0xff500000)
//Bit 31:29        reg_hist_slc_num          // unsigned ,    RW, default = 1
//Bit 28:16        reg_hist_slc0_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc0_x_ed        // unsigned ,    RW, default = 239
#define VDIN1_A9_HDR2_HIST_SLC_X_ST_ED_1              ((0x266d  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_hist_slc1_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc1_x_ed        // unsigned ,    RW, default = 239
#define VDIN1_A9_HDR2_HIST_SLC_X_ST_ED_2              ((0x266e  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_hist_slc2_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc2_x_ed        // unsigned ,    RW, default = 239
#define VDIN1_A9_HDR2_HIST_SLC_X_ST_ED_3              ((0x266f  << 2) + 0xff500000)
//Bit 31:29        reserved
//Bit 28:16        reg_hist_slc3_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc3_x_ed        // unsigned ,    RW, default = 239
#define VDIN1_A9_HDR2_ADPSCL1_COEF0                   ((0x2670  << 2) + 0xff500000)
//Bit 31:28        reserved
//Bit 27:16        reg_adpscl_ys_coef1_1     // unsigned ,    RW, default = 1024
//Bit 15:12        reserved
//Bit 11: 0        reg_adpscl_ys_coef1_0     // unsigned ,    RW, default = 1024
#define VDIN1_A9_HDR2_ADPSCL1_COEF1                   ((0x2671  << 2) + 0xff500000)
//Bit 31:16        reg_bypass_ootf2_gain     // unsigned ,    RW, default = 51200
//Bit 15:12        reserved
//Bit 11: 0        reg_adpscl_ys_coef1_2     // unsigned ,    RW, default = 1024
#define VDIN1_A9_HDR2_RGB_GM_CTRL                     ((0x2672  << 2) + 0xff500000)
//Bit 31: 3        reserved
//Bit  2           reg_adpscl_mode_gm        // unsigned ,    RW, default = 1
//Bit  1           reg_rgb_gm_mode           // unsigned ,    RW, default = 0
//Bit  0           reg_rgb_gm_en             // unsigned ,    RW, default = 0

/* a9 newly added registers end */

#endif /* __VDIN_A9_REGS_H */
