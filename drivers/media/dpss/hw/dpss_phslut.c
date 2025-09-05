// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "dpss_phslut.h"

void dpss_phs_lut_ini(enum DPSS_FRC_RATIO frc_ratio_mode, enum DPSS_FILM_MODE film_mode)
{
	// u32 input_n = 0;
	// u32 output_m = 0;
	u32 phase_table_num = 0;
	u64 phs_lut_table[32];
	u32 reg_inp_ofrm_loop_tab = 0;
	u32 reg_dae_loop_step_tab[8];
	u32 loop_step[32];
	int i;

	if (frc_ratio_mode == DPSS_FRC_RATIO_1_2) {
		//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
		//mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {
		//	1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2,
		//	4'h1     ,8'h00   ,8'h40   ,8'h00     ,8'h00
		//};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h40   ,8'h40   ,8'h00     ,8'h01};
		phs_lut_table[0] = 0x13232100400000;
		phs_lut_table[1] = 0x03232140400001;
		// input_n          = 1;
		// output_m         = 2;
		phase_table_num = 2;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_1_4) {
		//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
		//mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2,
		//	4'h1     ,8'h00   ,8'h40   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2,
		//	4'h1     ,8'h40   ,8'h40   ,8'h00     ,8'h01};
		phs_lut_table[0] = 0x13232100400000;
		phs_lut_table[1] = 0x03232120200001;
		phs_lut_table[2] = 0x03232140400002;
		phs_lut_table[3] = 0x03232160600003;
		// input_n          = 1;
		// output_m         = 4;
		phase_table_num = 4;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_2_3) {
		//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
		//mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h00   ,8'd85   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'd85   ,8'd85   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'd42   ,8'd42   ,8'h00     ,8'h02};
		phs_lut_table[0] = 0x13232100550000;
		phs_lut_table[1] = 0x03232155550001;
		phs_lut_table[2] = 0x1323212a2a0002;
		// input_n          = 2;
		// output_m         = 3;
		phase_table_num = 3;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_2_5) {
		//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
		//mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h00   ,8'd51   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'd51   ,8'd51   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'd102  ,8'd102  ,8'h00     ,8'h02};
		//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'd25   ,8'd25   ,8'h00     ,8'h03};
		//phs_lut_table[4] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'd76   ,8'd76   ,8'h00     ,8'h04};
		phs_lut_table[0] = 0x13232100330000;
		phs_lut_table[1] = 0x03232133330001;
		phs_lut_table[2] = 0x03232166660002;
		phs_lut_table[3] = 0x13232119190003;
		phs_lut_table[4] = 0x0323214c4c0004;
		// input_n          = 2;
		// output_m         = 5;
		phase_table_num = 5;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_5_6) {
		//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
		//mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h00   ,8'h6a   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h6a   ,8'h6a   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h55   ,8'h55   ,8'h00     ,8'h02};
		//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h40   ,8'h40   ,8'h00     ,8'h03};
		//phs_lut_table[4] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h2a   ,8'h2a   ,8'h00     ,8'h04};
		//phs_lut_table[5] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h15   ,8'h15   ,8'h00     ,8'h05};
		phs_lut_table[0] = 0x132321006a0000;
		phs_lut_table[1] = 0x0323216a6a0001;
		phs_lut_table[2] = 0x13232155550002;
		phs_lut_table[3] = 0x13232140400003;
		phs_lut_table[4] = 0x1323212a2a0004;
		phs_lut_table[5] = 0x13232115150005;
		// input_n          = 5;
		// output_m         = 6;
		phase_table_num = 6;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_5_12) {
		//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
		//mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h00   ,8'h35   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h35   ,8'h35   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h6a   ,8'h6a   ,8'h00     ,8'h02};
		//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h20   ,8'h20   ,8'h00     ,8'h03};
		//phs_lut_table[4] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h55   ,8'h55   ,8'h00     ,8'h04};
		//phs_lut_table[5] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h0a   ,8'h0a   ,8'h00     ,8'h05};
		//phs_lut_table[6] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h40   ,8'h40   ,8'h00     ,8'h06};
		//phs_lut_table[7] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h75   ,8'h75   ,8'h00     ,8'h07};
		//phs_lut_table[8] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h2a   ,8'h2a   ,8'h00     ,8'h08};
		//phs_lut_table[9] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h60   ,8'h60   ,8'h00     ,8'h09};
		//phs_lut_table[10]= {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h15   ,8'h15   ,8'h00     ,8'h0a};
		//phs_lut_table[11]= {1'h0     ,4'h3      ,4'h2      ,4'h3     ,
		//	4'h2     ,4'h1     ,8'h4a   ,8'h4a   ,8'h00     ,8'h0b};
		phs_lut_table[0] = 0x13232100350000;
		phs_lut_table[1] = 0x03232135350001;
		phs_lut_table[2] = 0x0323216a6a0002;
		phs_lut_table[3] = 0x13232120200003;
		phs_lut_table[4] = 0x03232155550004;
		phs_lut_table[5] = 0x1323210a0a0005;
		phs_lut_table[6] = 0x03232140400006;
		phs_lut_table[7] = 0x03232175750007;
		phs_lut_table[8] = 0x1323212a2a0008;
		phs_lut_table[9] = 0x03232160600009;
		phs_lut_table[10] = 0x1323211515000a;
		phs_lut_table[11] = 0x0323214a4a000b;
		// input_n          = 5;
		// output_m         = 12;
		phase_table_num = 12;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_1_1) {
		if (film_mode == DPSS_VIDEO) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,
			//	4'h2     ,4'h1     ,8'h00   ,8'h00  ,8'h00     ,8'h00};
			phs_lut_table[0] = 0x13232100000000;
			phase_table_num = 1;
		} else if (film_mode == DPSS_FILM22) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			// phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h4     ,
			//	4'h3     ,4'h1     ,8'd0   ,8'd64   ,8'h01     ,8'h00};
			// phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h5     ,
			//	4'h3     ,4'h2     ,8'd64  ,8'd64   ,8'h00     ,8'h00};
			phs_lut_table[0] = 0x13243100400100;
			phs_lut_table[1] = 0x03253240400000;
			phase_table_num = 2;
		} else if (film_mode == DPSS_FILM32) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h5     ,
			//	4'h4     ,4'h1     ,8'h00   ,8'h33   ,8'h01     ,8'h00};
			//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h6     ,
			//	4'h4     ,4'h2     ,8'h33   ,8'h33   ,8'h02     ,8'h00};
			//phs_lut_table[2] = {1'h0     ,4'h4      ,4'h3      ,4'h7     ,
			//	4'h4     ,4'h3     ,8'h66   ,8'h66   ,8'h03     ,8'h00};
			//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h5     ,
			//	4'h4     ,4'h2     ,8'h19   ,8'h19   ,8'h04     ,8'h00};
			//phs_lut_table[4] = {1'h0     ,4'h3      ,4'h2      ,4'h6     ,
			//	4'h4     ,4'h3     ,8'h4c   ,8'h4c   ,8'h00     ,8'h00};
			phs_lut_table[0] = 0x13254100330100;
			phs_lut_table[1] = 0x03264233330200;
			phs_lut_table[2] = 0x04374366660300;
			phs_lut_table[3] = 0x13254219190400;
			phs_lut_table[4] = 0x0326434c4c0000;
			phase_table_num = 5;
		} else if (film_mode == DPSS_FILM3223) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] = { 1'd1,   4'd3,      4'd2,      4'd7,     4'd4,
			//	4'd1,     8'd0,   8'd51,     8'd1,    8'h00};
			//phs_lut_table[1] = { 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,
			//	4'd2,     8'd51,  8'd51,     8'd2,    8'h00};
			//phs_lut_table[2] = { 1'd0,   4'd4,      4'd3,      4'd9,     4'd6,
			//	4'd3,     8'd102, 8'd102,    8'd3,    8'h00};
			//phs_lut_table[3] = { 1'd1,   4'd3,      4'd2,      4'd7,     4'd4,
			//	4'd2,     8'd25,  8'd25,     8'd4,    8'h00};
			//phs_lut_table[4] = { 1'd0,   4'd4,      4'd3,      4'd8,     4'd5,
			//	4'd3,     8'd76,  8'd76,     8'd5,    8'h00};
			//phs_lut_table[5] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,
			//	4'd2,     8'd0,   8'd51,     8'd6,    8'h00};
			//phs_lut_table[6] = { 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd3,     8'd51,  8'd51,     8'd7,    8'h00};
			//phs_lut_table[7] = { 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd4,     8'd102, 8'd102,    8'd8,    8'h00};
			//phs_lut_table[8] = { 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd2,     8'd25,  8'd25,     8'd9,    8'h00};
			//phs_lut_table[9] = { 1'd0,   4'd3,      4'd2,      4'd8,     4'd6,
			//	4'd3,     8'd76,  8'd76,     8'd0,    8'h00};
			phs_lut_table[0] = 0x13274100330100;
			phs_lut_table[1] = 0x03285233330200;
			phs_lut_table[2] = 0x04396366660300;
			phs_lut_table[3] = 0x13274219190400;
			phs_lut_table[4] = 0x0438534c4c0500;
			phs_lut_table[5] = 0x13264200330600;
			phs_lut_table[6] = 0x03275333330700;
			phs_lut_table[7] = 0x04386466660800;
			phs_lut_table[8] = 0x13275219190900;
			phs_lut_table[9] = 0x0328634c4c0000;
			phase_table_num = 10;
		} else if (film_mode == DPSS_FILM2224) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] = { 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd1,    8'd0,    8'd51,     8'd1,    8'h00};
			//phs_lut_table[1] = { 1'd0,   4'd3,      4'd2,      4'd8,     4'd6,
			//	4'd2,    8'd51,   8'd51,     8'd2,    8'h00};
			//phs_lut_table[2] = { 1'd0,   4'd4,      4'd3,      4'd9,     4'd7,
			//	4'd3,    8'd102,  8'd102,    8'd3,    8'h00};
			//phs_lut_table[3] = { 1'd1,   4'd3,      4'd2,      4'd8,     4'd4,
			//	4'd2,    8'd25,   8'd25,     8'd4,    8'h00};
			//phs_lut_table[4] = { 1'd0,   4'd4,      4'd3,      4'd9,     4'd5,
			//	4'd3,    8'd76,   8'd76,     8'd5,    8'h00};
			//phs_lut_table[5] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,
			//	4'd2,    8'd0,    8'd51,     8'd6,    8'h00};
			//phs_lut_table[6] = { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,
			//	4'd3,    8'd51,   8'd51,     8'd7,    8'h00};
			//phs_lut_table[7] = { 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd4,    8'd102,  8'd102,    8'd8,    8'h00};
			//phs_lut_table[8] = { 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd3,    8'd25,   8'd25,     8'd9,    8'h00};
			//phs_lut_table[9] = { 1'd0,   4'd3,      4'd2,      4'd8,     4'd6,
			//	4'd4,    8'd76,   8'd76,     8'd0,    8'h00};
			phs_lut_table[0] = 0x13275100330100;
			phs_lut_table[1] = 0x03286233330200;
			phs_lut_table[2] = 0x04397366660300;
			phs_lut_table[3] = 0x13284219190400;
			phs_lut_table[4] = 0x0439534c4c0500;
			phs_lut_table[5] = 0x13264200330600;
			phs_lut_table[6] = 0x04375333330700;
			phs_lut_table[7] = 0x04386466660800;
			phs_lut_table[8] = 0x13275319190900;
			phs_lut_table[9] = 0x0328644c4c0000;
			phase_table_num = 10;
		} else if (film_mode == DPSS_FILM32322) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,
			//	4'd1,     8'd0,   8'd53,     8'd1,      8'h00};
			//phs_lut_table[1] =  { 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd2,     8'd53,   8'd53,     8'd2,     8'h00};
			//phs_lut_table[2] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd3,     8'd106,   8'd106,     8'd3,   8'h00};
			//phs_lut_table[3] =  { 1'd1,   4'd3,      4'd2,      4'd7,     4'd4,
			//	4'd2,     8'd32,   8'd32,     8'd4,     8'h00};
			//phs_lut_table[4] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd5,
			//	4'd3,     8'd85,   8'd85,     8'd5,     8'h00};
			//phs_lut_table[5] =  { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,
			//	4'd2,     8'd10,   8'd10,     8'd6,     8'h00};
			//phs_lut_table[6] =  { 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd3,     8'd64,   8'd64,     8'd7,     8'h00};
			//phs_lut_table[7] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd4,     8'd117,   8'd117,     8'd8,   8'h00};
			//phs_lut_table[8] =  { 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd2,     8'd42,   8'd42,     8'd9,     8'h00};
			//phs_lut_table[9] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd3,     8'd96,   8'd96,     8'd10,    8'h00};
			//phs_lut_table[10] = { 1'd1,   4'd3,      4'd2,      4'd7,     4'd4,
			//	4'd2,     8'd21,   8'd21,     8'd11,    8'h00};
			//phs_lut_table[11] = { 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,
			//	4'd3,     8'd74,   8'd74,     8'd0,     8'h00};
			phs_lut_table[0] = 0x13264100350100;
			phs_lut_table[1] = 0x03275235350200;
			phs_lut_table[2] = 0x0438636a6a0300;
			phs_lut_table[3] = 0x13274220200400;
			phs_lut_table[4] = 0x04385355550500;
			phs_lut_table[5] = 0x1326420a0a0600;
			phs_lut_table[6] = 0x03275340400700;
			phs_lut_table[7] = 0x04386475750800;
			phs_lut_table[8] = 0x1327522a2a0900;
			phs_lut_table[9] = 0x04386360600a00;
			phs_lut_table[10] = 0x13274215150b00;
			phs_lut_table[11] = 0x0328534a4a0000;
			phase_table_num = 12;
		} else if (film_mode == DPSS_FILM44) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//	mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd9,     4'd5,
			//4'd1,     8'd0,   8'd32,     8'd1,    8'h00};
			//phs_lut_table[1] =  { 1'd0,   4'd3,      4'd2,      4'd10,     4'd6,
			//4'd2,     8'd32,   8'd32,     8'd2,   8'h00};
			//phs_lut_table[2] =  { 1'd0,   4'd3,      4'd2,      4'd11,     4'd7,
			//4'd3,     8'd64,   8'd64,     8'd3,   8'h00};
			//phs_lut_table[3] =  { 1'd0,   4'd3,      4'd2,      4'd12,     4'd8,
			//4'd4,     8'd96,   8'd96,     8'd0,   8'h00};
			phs_lut_table[0] = 0x13295100200100;
			phs_lut_table[1] = 0x032a6220200200;
			phs_lut_table[2] = 0x032b7340400300;
			phs_lut_table[3] = 0x032c8460600000;
			phase_table_num = 4;
		} else if (film_mode == DPSS_FILM21111) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,
			//	4'd1,     8'd0,   8'd106,     8'd1,     8'h00};
			//phs_lut_table[1] =  { 1'd0,   4'd4,      4'd3,      4'd5,     4'd4,
			//	4'd2,     8'd106,   8'd106,     8'd2,   8'h00};
			//phs_lut_table[2] =  { 1'd1,   4'd4,      4'd3,      4'd5,     4'd3,
			//	4'd2,     8'd85,   8'd85,     8'd3,     8'h00};
			//phs_lut_table[3] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,
			//	4'd2,     8'd64,   8'd64,     8'd4,     8'h00};
			//phs_lut_table[4] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,
			//	4'd2,     8'd42,   8'd42,     8'd5,     8'h00};
			//phs_lut_table[5] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,
			//	4'd2,     8'd21,   8'd21,     8'd0,     8'h00};
			phs_lut_table[0] = 0x132431006a0100;
			phs_lut_table[1] = 0x0435426a6a0200;
			phs_lut_table[2] = 0x14353255550300;
			phs_lut_table[3] = 0x14343240400400;
			phs_lut_table[4] = 0x1434322a2a0500;
			phs_lut_table[5] = 0x13243215150000;
			phase_table_num = 6;
		} else if (film_mode == DPSS_FILM23322) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd7,     4'd4,
			//	4'd1,     8'd0,   8'd53,     8'd1,       8'h00};
			//phs_lut_table[1] =  { 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,
			//	4'd2,     8'd53,   8'd53,     8'd2,      8'h00};
			//phs_lut_table[2] =  { 1'd0,   4'd4,      4'd3,      4'd9,     4'd6,
			//		4'd3,     8'd106,   8'd106,     8'd3,    8'h00};
			//phs_lut_table[3] =  { 1'd1,   4'd3,      4'd2,      4'd7,     4'd4,
			//	4'd2,     8'd32,   8'd32,     8'd4,      8'h00};
			//phs_lut_table[4] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd5,
			//	4'd3,     8'd85,   8'd85,     8'd5,      8'h00};
			//phs_lut_table[5] =  { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,
			//	4'd2,     8'd10,   8'd10,     8'd6,      8'h00};
			//phs_lut_table[6] =  { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,
			//	4'd3,     8'd64,   8'd64,     8'd7,      8'h00};
			//phs_lut_table[7] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd4,     8'd117,   8'd117,     8'd8,    8'h00};
			//phs_lut_table[8] =  { 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd3,     8'd42,   8'd42,     8'd9,      8'h00};
			//phs_lut_table[9] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd4,     8'd96,   8'd96,     8'd10,     8'h00};
			//phs_lut_table[10]= { 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd2,     8'd21,   8'd21,     8'd11,     8'h00};
			//phs_lut_table[11]= { 1'd0,   4'd3,      4'd2,      4'd8,     4'd6,
			//	4'd3,     8'd74,   8'd74,     8'd0,      8'h00};
			phs_lut_table[0] = 0x13274100350100;
			phs_lut_table[1] = 0x03285235350200;
			phs_lut_table[2] = 0x0439636a6a0300;
			phs_lut_table[3] = 0x13274220200400;
			phs_lut_table[4] = 0x04385355550500;
			phs_lut_table[5] = 0x1326420a0a0600;
			phs_lut_table[6] = 0x04375340400700;
			phs_lut_table[7] = 0x04386475750800;
			phs_lut_table[8] = 0x1327532a2a0900;
			phs_lut_table[9] = 0x04386460600a00;
			phs_lut_table[10] = 0x13275215150b00;
			phs_lut_table[11] = 0x0328634a4a0000;
			phase_table_num = 12;
		} else if (film_mode == DPSS_FILM2111) {
			//phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff
			//mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,
			//	4'd1,     8'd0,   8'd102,    8'd1,      8'h00};
			//phs_lut_table[1] =  { 1'd0,   4'd4,      4'd3,      4'd5,     4'd4,
			//	4'd2,     8'd102, 8'd102,    8'd2,      8'h00};
			//phs_lut_table[2] =  { 1'd1,   4'd4,      4'd3,      4'd5,     4'd3,
			//	4'd2,     8'd76,  8'd76,     8'd3,      8'h00};
			//phs_lut_table[3] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,
			//	4'd2,     8'd51,  8'd51,     8'd4,      8'h00};
			//phs_lut_table[4] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,
			//	4'd2,     8'd25,  8'd25,     8'd0,      8'h00};
			phs_lut_table[0] = 0x13243100660100;
			phs_lut_table[1] = 0x04354266660200;
			phs_lut_table[2] = 0x1435324c4c0300;
			phs_lut_table[3] = 0x14343233330400;
			phs_lut_table[4] = 0x13243219190000;
			phase_table_num = 5;
		} else if (film_mode == DPSS_FILM22224) {
			//candence=22224(10), table_cnt=  12, match_data=0xaa8
			//pha_start, plogo_diff, clogo_diff, pfrm_diff, cfrm_diff, nfrm_diff,
			//mc_phase, me_phase, film_phase, frc_phase
			//phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd1,     8'd0,   8'd53,     8'd1,       8'd0};
			//phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd8,     4'd6,
			//	4'd2,     8'd53,   8'd53,     8'd2,       8'd0};
			//phs_lut_table[2]={ 1'd0,   4'd4,      4'd3,      4'd9,     4'd7,
			//	4'd3,     8'd106,   8'd106,     8'd3,       8'd0};
			//phs_lut_table[3]={ 1'd1,   4'd3,      4'd2,      4'd8,     4'd4,
			//	4'd2,     8'd32,   8'd32,     8'd4,       8'd0};
			//phs_lut_table[4]={ 1'd0,   4'd4,      4'd3,      4'd9,     4'd5,
			//	4'd3,     8'd85,   8'd85,     8'd5,       8'd0};
			//phs_lut_table[5]={ 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,
			//	4'd2,     8'd10,   8'd10,     8'd6,       8'd0};
			//phs_lut_table[6]={ 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,
			//	4'd3,     8'd64,   8'd64,     8'd7,       8'd0};
			//phs_lut_table[7]={ 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd4,     8'd117,   8'd117,     8'd8,       8'd0};
			//phs_lut_table[8]={ 1'd1,   4'd4,      4'd3,      4'd7,     4'd5,
			//	4'd3,     8'd42,   8'd42,     8'd9,       8'd0};
			//phs_lut_table[9]={ 1'd0,   4'd4,      4'd3,      4'd8,     4'd6,
			//	4'd4,     8'd96,   8'd96,     8'd10,       8'd0};
			//phs_lut_table[10]={ 1'd1,   4'd3,      4'd2,      4'd7,     4'd5,
			//	4'd3,     8'd21,   8'd21,     8'd11,       8'd0};
			//phs_lut_table[11]={ 1'd0,   4'd3,      4'd2,      4'd8,     4'd6,
			//		4'd4,     8'd74,   8'd74,     8'd0,       8'd0};
			phs_lut_table[0] = 0x13275100350100;
			phs_lut_table[1] = 0x03286235350200;
			phs_lut_table[2] = 0x0439736a6a0300;
			phs_lut_table[3] = 0x13284220200400;
			phs_lut_table[4] = 0x04395355550500;
			phs_lut_table[5] = 0x1326420a0a0600;
			phs_lut_table[6] = 0x04375340400700;
			phs_lut_table[7] = 0x04386475750800;
			phs_lut_table[8] = 0x1437532a2a0900;
			phs_lut_table[9] = 0x04386460600a00;
			phs_lut_table[10] = 0x13275315150b00;
			phs_lut_table[11] = 0x0328644a4a0000;
			phase_table_num = 12;
		} else if (film_mode == DPSS_FILM33) {
			//candence=33(11),table_cnt=   3, match_data=0x4
			//pha_start, plogo_diff, clogo_diff,  pfrm_diff, cfrm_diff, nfrm_diff,
			//mc_phase, me_phase, film_phase, frc_phase
			//phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd7,     4'd4,
			//4'd1,     8'd0,   8'd42,     8'd1,       8'd0};
			//phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,
			//4'd2,     8'd42,   8'd42,     8'd2,       8'd0};
			//phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd9,     4'd6,
			//4'd3,     8'd85,   8'd85,     8'd0,       8'd0};
			phs_lut_table[0] = 0x132741002a0100;
			phs_lut_table[1] = 0x0328522a2a0200;
			phs_lut_table[2] = 0x03296355550000;
			phase_table_num = 3;
		} else if (film_mode == DPSS_FILM334) {
			//candence=334(12), table_cnt=  10, match_data=0x248
			//pha_start, plogo_diff, clogo_diff, pfrm_diff, cfrm_diff, nfrm_diff,
			//mc_phase, me_phase, film_phase, frc_phase
			//phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd8,     4'd5,
			//	4'd1,     8'd0,   8'd38,     8'd1,       8'd0};
			//phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd9,     4'd6,
			//	4'd2,     8'd38,   8'd38,     8'd2,       8'd0};
			//phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd10,     4'd7,
			//	4'd3,     8'd76,   8'd76,     8'd3,       8'd0};
			//phs_lut_table[3]={ 1'd0,   4'd4,      4'd3,      4'd11,     4'd8,
			//	4'd4,     8'd115,   8'd115,     8'd4,       8'd0};
			//phs_lut_table[4]={ 1'd1,   4'd3,      4'd2,      4'd9,     4'd5,
			//	4'd2,     8'd25,   8'd25,     8'd5,       8'd0};
			//phs_lut_table[5]={ 1'd0,   4'd3,      4'd2,      4'd10,     4'd6,
			//	4'd3,     8'd64,   8'd64,     8'd6,       8'd0};
			//phs_lut_table[6]={ 1'd0,   4'd4,      4'd3,      4'd11,     4'd7,
			//	4'd4,     8'd102,   8'd102,     8'd7,       8'd0};
			//phs_lut_table[7]={ 1'd1,   4'd3,      4'd2,      4'd8,     4'd5,
			//	4'd2,     8'd12,   8'd12,     8'd8,       8'd0};
			//phs_lut_table[8]={ 1'd0,   4'd3,      4'd2,      4'd9,     4'd6,
			//	4'd3,     8'd51,   8'd51,     8'd9,       8'd0};
			//phs_lut_table[9]={ 1'd0,   4'd3,      4'd2,      4'd10,     4'd7,
			//	4'd4,     8'd89,   8'd89,     8'd0,       8'd0};
			phs_lut_table[0] = 0x13285100260100;
			phs_lut_table[1] = 0x03296226260200;
			phs_lut_table[2] = 0x032a734c4c0300;
			phs_lut_table[3] = 0x043b8473730400;
			phs_lut_table[4] = 0x13295219190500;
			phs_lut_table[5] = 0x032a6340400600;
			phs_lut_table[6] = 0x043b7466660700;
			phs_lut_table[7] = 0x1328520c0c0800;
			phs_lut_table[8] = 0x03296333330900;
			phs_lut_table[9] = 0x032a7459590000;
			phase_table_num = 10;
		} else if (film_mode == DPSS_FILM55) {
			//candence=55(13), table_cnt=   5, match_data=0x10
			//pha_start, plogo_diff, clogo_diff, pfrm_diff, frm_diff, nfrm_diff,
			//mc_phase, me_phase, film_phase, frc_phase
			//phs_lut_table[0]={ 1'd1,      4'd3,      4'd2,      4'd8,     4'd13,
			//	4'd2,     8'd0,    8'd25,     8'd1,     8'd0};
			//phs_lut_table[1]={ 1'd0,      4'd3,      4'd2,      4'd9,     4'd14,
			//	4'd3,     8'd25,   8'd25,     8'd2,     8'd0};
			//phs_lut_table[2]={ 1'd0,      4'd3,      4'd2,      4'd12,     4'd1,
			//	4'd6,     8'd51,   8'd51,     8'd3,     8'd0};
			//phs_lut_table[3]={ 1'd0,      4'd3,      4'd2,      4'd14,    4'd3,
			//	4'd8,     8'd76,   8'd76,     8'd4,     8'd0};
			//phs_lut_table[4]={ 1'd0,      4'd3,      4'd2,      4'd1,    4'd6,
			//	4'd11,     8'd102,  8'd102,    8'd0,     8'd0};
			phs_lut_table[0] = 0x1328d200190100;
			phs_lut_table[1] = 0x0329e319190200;
			phs_lut_table[2] = 0x032c1633330300;
			phs_lut_table[3] = 0x032e384c4c0400;
			phs_lut_table[4] = 0x03216b66660000;
			phase_table_num = 5;
		} else if (film_mode == DPSS_FILM64) {
			//candence=64(14), table_cnt=  10, match_data=0x220
			//pha_start, plogo_diff, clogo_diff, pfrm_diff, cfrm_diff, nfrm_diff,
			//mc_phase, me_phase, film_phase, frc_phase
			//phs_lut_table[0]={ 1'd1,      4'd3,      4'd2,      4'd3,     4'd13,
			//	4'd5,     8'd0,    8'd25,     8'd1,       8'd0};
			//phs_lut_table[1]={ 1'd0,      4'd3,      4'd2,      4'd9,     4'd3,
			//	4'd11,     8'd25,   8'd25,     8'd2,       8'd0};
			//phs_lut_table[2]={ 1'd0,      4'd3,      4'd2,      4'd14,    4'd8,
			//	4'd0,     8'd51,   8'd51,     8'd3,       8'd0};
			//phs_lut_table[3]={ 1'd0,      4'd3,      4'd2,      4'd4,    4'd14,
			//	4'd6,     8'd76,   8'd76,     8'd4,       8'd0};
			//phs_lut_table[4]={ 1'd0,      4'd4,      4'd3,      4'd9,    4'd3,
			//	4'd11,     8'd102,  8'd102,    8'd5,       8'd0};
			//phs_lut_table[5]={ 1'd1,      4'd3,      4'd2,      4'd6,     4'd14,
			//	4'd8,     8'd0,    8'd25,     8'd6,       8'd0};
			//phs_lut_table[6]={ 1'd0,      4'd3,      4'd2,      4'd9,     4'd1,
			//	4'd11,     8'd25,   8'd25,     8'd7,       8'd0};
			//phs_lut_table[7]={ 1'd0,      4'd3,      4'd2,      4'd12,    4'd4,
			//	4'd14,     8'd51,   8'd51,     8'd8,       8'd0};
			//phs_lut_table[8]={ 1'd0,      4'd3,      4'd2,      4'd2,    4'd10,
			//	4'd4,     8'd76,   8'd76,     8'd9,       8'd0};
			//phs_lut_table[9]={ 1'd0,      4'd3,      4'd2,      4'd6,    4'd14,
			//	4'd8,     8'd102,  8'd102,    8'd0,       8'd0};
			phs_lut_table[0] = 0x1323d500190100;
			phs_lut_table[1] = 0x03293b19190200;
			phs_lut_table[2] = 0x032e8033330300;
			phs_lut_table[3] = 0x0324e64c4c0400;
			phs_lut_table[4] = 0x04393b66660500;
			phs_lut_table[5] = 0x1326e800190600;
			phs_lut_table[6] = 0x03291b19190700;
			phs_lut_table[7] = 0x032c4e33330800;
			phs_lut_table[8] = 0x0322a44c4c0900;
			phs_lut_table[9] = 0x0326e866660000;
			phase_table_num = 10;
		} else if (film_mode == DPSS_FILM66) {
			//candence=66(15),table_cnt=   6,match_data=0x20
			//pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,
			//mc_phase, me_phase, film_phase, frc_phase
			//phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd9,     4'd8,
			//4'd7,     8'd0,    8'd21,     8'd1,       8'd0};
			//phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd14,    4'd13,
			//4'd12,    8'd21,   8'd21,     8'd2,       8'd0};
			//phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd5,     4'd4,
			//4'd3,     8'd42,   8'd42,     8'd3,       8'd0};
			//phs_lut_table[3]={ 1'd0,   4'd3,      4'd2,      4'd9,     4'd8,
			//4'd7,     8'd64,   8'd64,     8'd4,       8'd0};
			//phs_lut_table[4]={ 1'd0,   4'd3,      4'd2,      4'd13,    4'd12,
			//4'd11,    8'd85,   8'd85,     8'd5,       8'd0};
			//phs_lut_table[5]={ 1'd0,   4'd3,      4'd2,      4'd3,     4'd2,
			//4'd1,     8'd106,  8'd106,    8'd0,       8'd0};
			phs_lut_table[0] = 0x13298700150100;
			phs_lut_table[1] = 0x032edc15150200;
			phs_lut_table[2] = 0x0325432a2a0300;
			phs_lut_table[3] = 0x03298740400400;
			phs_lut_table[4] = 0x032dcb55550500;
			phs_lut_table[5] = 0x0323216a6a0000;
			phase_table_num = 6;
		} else if (film_mode == DPSS_FILM87) {
			//candence=87(16),table_cnt=  15,match_data=0x4080
			//pha_start, plogo_diff, clogo_diff, pfrm_diff
			// cfrm_diff, nfrm_diff, mc_phase, me_phase, film_phase, frc_phase
			//phs_lut_table[0]= { 1'd1,   4'd3,      4'd2,      4'd7,     4'd14,
			//	4'd13,     8'd0,    8'd17,    8'd1,     8'd0};
			//phs_lut_table[1]= { 1'd0,   4'd3,      4'd2,      4'd6,    4'd13,
			//	4'd12,     8'd17,   8'd17,    8'd2,     8'd0};
			//phs_lut_table[2]= { 1'd0,   4'd3,      4'd2,      4'd4,    4'd11,
			//	4'd10,     8'd34,   8'd34,    8'd3,     8'd0};
			//phs_lut_table[3]= { 1'd0,   4'd3,      4'd2,      4'd10,    4'd1,
			//	4'd0,     8'd51,   8'd51,    8'd4,     8'd0};
			//phs_lut_table[4]= { 1'd0,   4'd3,      4'd2,      4'd4,     4'd11,
			//	4'd10,    8'd68,   8'd68,    8'd5,     8'd0};
			//phs_lut_table[5]= { 1'd0,   4'd3,      4'd2,      4'd10,     4'd1,
			//	4'd0,    8'd85,   8'd85,    8'd6,     8'd0};
			//phs_lut_table[6]= { 1'd0,   4'd3,      4'd2,      4'd3,     4'd10,
			//	4'd9,     8'd102,  8'd102,   8'd7,     8'd0};
			//phs_lut_table[7]= { 1'd0,   4'd4,      4'd3,      4'd12,    4'd3,
			//	4'd2,     8'd119,  8'd119,   8'd8,     8'd0};
			//phs_lut_table[8]= { 1'd1,   4'd3,      4'd2,      4'd10,     4'd9,
			//	4'd0,     8'd8,    8'd8,     8'd9,     8'd0};
			//phs_lut_table[9]= { 1'd0,   4'd3,      4'd2,      4'd3,    4'd2,
			//	4'd9,     8'd25,   8'd25,    8'd10,    8'd0};
			//phs_lut_table[10]={ 1'd0,   4'd3,      4'd2,      4'd11,    4'd10,
			//	4'd1,     8'd42,   8'd42,    8'd11,    8'd0};
			//phs_lut_table[11]={ 1'd0,   4'd3,      4'd2,      4'd3,    4'd2,
			//	4'd9,    8'd59,   8'd59,    8'd12,    8'd0};
			//phs_lut_table[12]={ 1'd0,   4'd3,      4'd2,      4'd10,     4'd9,
			//	4'd0,    8'd76,   8'd76,    8'd13,    8'd0};
			//phs_lut_table[13]={ 1'd0,   4'd3,      4'd2,      4'd3,     4'd2,
			//	4'd9,     8'd93,   8'd93,    8'd14,    8'd0};
			//phs_lut_table[14]={ 1'd0,   4'd3,      4'd12,     4'd11,     4'd10,
			//	4'd1,     8'd110,  8'd110,   8'd0,     8'd0};
			phs_lut_table[0] = 0x1327ed00110100;
			phs_lut_table[1] = 0x0326dc11110200;
			phs_lut_table[2] = 0x0324ba22220300;
			phs_lut_table[3] = 0x032a1033330400;
			phs_lut_table[4] = 0x0324ba44440500;
			phs_lut_table[5] = 0x032a1055550600;
			phs_lut_table[6] = 0x0323a966660700;
			phs_lut_table[7] = 0x043c3277770800;
			phs_lut_table[8] = 0x132a9008080900;
			phs_lut_table[9] = 0x03232919190a00;
			phs_lut_table[10] = 0x032ba12a2a0b00;
			phs_lut_table[11] = 0x0323293b3b0c00;
			phs_lut_table[12] = 0x032a904c4c0d00;
			phs_lut_table[13] = 0x0323295d5d0e00;
			phs_lut_table[14] = 0x03cba16e6e0000;
			phase_table_num = 15;
		} else if (film_mode == DPSS_FILM212) {
			//candence=212(17),table_cnt=   5,match_data=0x1a
			//pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,
			//mc_phase,me_phase,film_phase,frc_phase
			//phs_lut_table[0]={ 1'd1,4'd3,4'd2,4'd5,4'd3,4'd1,8'd0,8'd76,8'd1,8'd0};
			//phs_lut_table[1]={ 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,
			//	4'd2,     8'd76,   8'd76,     8'd2,       8'd0};
			//phs_lut_table[2]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd3,
			//	4'd2,     8'd25,   8'd25,     8'd3,       8'd0};
			//phs_lut_table[3]={ 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,
			//	/4'd3,     8'd102,   8'd102,     8'd4,       8'd0};
			//phs_lut_table[4]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,
			//	4'd2,     8'd51,   8'd51,     8'd0,       8'd0};
			phs_lut_table[0] = 0x132531004c0100;
			phs_lut_table[1] = 0x0436424c4c0200;
			phs_lut_table[2] = 0x13253219190300;
			phs_lut_table[3] = 0x04364366660400;
			phs_lut_table[4] = 0x13254233330000;
			phase_table_num = 5;
		} else {
			dbg_h2("==== USE ERROR CANDENCE ====");
			return;
		}
		// input_n          = 1;
		// output_m         = 1;
	} else {
		dbg_f2("%s un init frc_ratio_mode:%d, film_mode:%d\n",
			__func__, frc_ratio_mode, film_mode);
		return;
	}
	//init inp_ofrm_loop_tab from phs_lut
	for (i = 0; i < phase_table_num; i++)
		reg_inp_ofrm_loop_tab |= ((phs_lut_table[i] >> 52) & 0x1) << i;
	wr(FRC_AUTO_MODE_LOOP_TAB, reg_inp_ofrm_loop_tab);	//reg_inp_ofrm_loop_tab

	//init loop_step_tab from phs_lut
	u32 tmp0, tmp1;

	for (i = 0; i < 32; i++) {
		tmp0 = (phs_lut_table[i] >> 24) & 0xff;
		if (i < (phase_table_num - 1)) {
			tmp1 = (phs_lut_table[i + 1] >> 24) & 0xff;
			if (tmp1 > tmp0)
				loop_step[i] = tmp1 - tmp0;
			else
				loop_step[i] = tmp1 + 128 - tmp0;
		} else if (i == (phase_table_num - 1)) {
			loop_step[i] = 128 - tmp0;
		} else {
			loop_step[i] = 0;
		}
	}
	for (i = 0; i < 8; i++) {
		reg_dae_loop_step_tab[i] = (loop_step[4 * i + 3] << 24) |
		    (loop_step[4 * i + 2] << 16) |
		    (loop_step[4 * i + 1] << 8) | (loop_step[4 * i + 0] << 0);
		wr(FRC_AUTO_MODE_STEP_TAB0 + i, reg_dae_loop_step_tab[i]);	//ary addr change *4
	}			//otb

	//config_dae_ofrm_step_at_inp_site(loop_step[0]<<4);//reg_inp_ofrm_step.
	w_reg_bit(FRC_AUTO_MODE_ENABLE, 1, 0, 1);	//reg_frc_fbuf_ctrl_mode
	w_reg_bit(FRC_AUTO_MODE_ENABLE, 0, 4, 1);	//reg_inp_ofrm_idx_sel
}

void cfg_dpss_gmd_phs_lut(enum DPSS_FRC_RATIO frc_ratio_mode, u32 config_ini_en)
{
	//ary no use    u32 phs_tab0 = 0;
	//ary no use    u32 phs_tab1 = 0;
	u32 phs_ini = 0;
	u32 phs_step = 0;
	u32 dae_phs0 = 0;
	//ary no use    u32 phs_num = 0;
	if (frc_ratio_mode == DPSS_FRC_RATIO_1_1) {
		dae_phs0 = 0x0;
		phs_ini = 0x00000;
		phs_step = 0x80000;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_1_2) {
		dae_phs0 = 0x40;
		phs_ini = 0x00000;
		phs_step = 0x40000;
		//return;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_2_5) {
		//phs_tab0 = 0x34<<24 | 0x33 << 16 | 0x33 <<8 | 0x33;
		//phs_tab1 = 0x33;
		dae_phs0 = 0x33;
		phs_ini = 0x33333;
		phs_step = 0x33333;
		//phs_num  = 5;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_1_4) {
		//phs_tab0 = 0x20<<24 | 0x20 << 16 | 0x20 <<8 | 0x20;
		//phs_tab1 = 0x0;
		dae_phs0 = 0x20;
		phs_ini = 0x40000;
		phs_step = 0x20000;
		//phs_num  = 4;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_1_5) {
		//phs_tab0 = 0x19<<24 | 0x1a << 16 | 0x1a <<8 | 0x1a;
		//phs_tab1 = 0x19;
		dae_phs0 = 0x19;
		phs_ini = 0x33332;
		phs_step = 0x19999;
		//phs_num  = 5;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_1_6) {
		//phs_tab0 = 0x15<<24 | 0x15 << 16 | 0x16 <<8 | 0x15;
		//phs_tab1 = 0x15<<8  | 0x16;
		dae_phs0 = 0x15;
		phs_ini = 0x2aaaa;
		phs_step = 0x15555;
		//phs_num  = 6;
	} else if (frc_ratio_mode == DPSS_FRC_RATIO_5_12) {
		//phs_tab0 = 0x15<<24 | 0x15 << 16 | 0x16 <<8 | 0x15;
		//phs_tab1 = 0x15<<8  | 0x16;
		dae_phs0 = 0x35;
		phs_ini = 0x35555;
		phs_step = 0x35555;
		//phs_num  = 6;
	}
	//wr(FRC_AUTO_MODE_STEP_TAB0,phs_tab0);
	//wr(FRC_AUTO_MODE_STEP_TAB1,phs_tab1);
	//w_reg_bit(FRC_AUTO_MODE_LOOP_CTRL,phs_num,0,5);//reg_dae_loop_step_num
	w_reg_bit(FRC_REG_TOP_CTRL7, 2, 20, 2);	//reg_dae_step_sel
	config_dae_ofrm_step_at_dae_site(phs_step);	//reg_dae_ofrm_step
	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE, dae_phs0, 24, 8);	//todo
	// w_reg_bit(FRC_DPSS_LLM, 1, 2, 1);	//reg_less_rd_one
	w_reg_bit(FRC_AUTO_MODE_PHS_INIT, phs_ini, 4, 8);	// ini_phase
	w_reg_bit(FRC_AUTO_MODE_PHS_INIT, 1, 0, 1);	// ini_en pulse
}

u32 config_inp_loop_tab(enum DPSS_FILM_MODE film_mode, u32 film_phs,
			     u32 badedit_en)
{
	u32 inp_ofrm_loop_tab = 0;
	u32 inp_ofrm_vld;	//32b
	u32 film_hwfw_sel = (rd(FRC_REG_FILM_PHS_1) >> 16) & 0x1;
	//if(badedit_en==0) {
	if (film_hwfw_sel) {
		w_reg_bit(FRC_REG_PHS_TABLE, film_mode, 0, 8);	//fw
		dbg_h2("inp config film_mode = %d\n", film_mode);
	}
	//w_reg_bit(FRC_REG_PHS_TABLE,film_mode,8,8);//test
	//}
	switch (film_mode) {
	case DPSS_VIDEO:
		inp_ofrm_loop_tab = 0b1;
		break;		//VIDEO
	case DPSS_FILM22:
		inp_ofrm_loop_tab = 0b01;
		break;		//22
	case DPSS_FILM32:
		inp_ofrm_loop_tab = 0b00101;
		break;		//32
	case DPSS_FILM3223:
		inp_ofrm_loop_tab = 0b0010010101;
		break;		//3223
	case DPSS_FILM2224:
		inp_ofrm_loop_tab = 0b0001010101;
		break;		//2224
	case DPSS_FILM32322:
		inp_ofrm_loop_tab = 0b001010010101;
		break;		//32322
	case DPSS_FILM44:
		inp_ofrm_loop_tab = 0b0001;
		break;		//44
	case DPSS_FILM21111:
		inp_ofrm_loop_tab = 0b011111;
		break;		//21111
	case DPSS_FILM23322:
		inp_ofrm_loop_tab = 0b001001010101;
		break;		//23322
	case DPSS_FILM2111:
		inp_ofrm_loop_tab = 0b01111;
		break;		//2111
	case DPSS_FILM22224:
		inp_ofrm_loop_tab = 0b000101010101;
		break;		//22224
	case DPSS_FILM33:
		inp_ofrm_loop_tab = 0b001;
		break;		//33
	case DPSS_FILM334:
		inp_ofrm_loop_tab = 0b0001001001;
		break;		//334
	case DPSS_FILM55:
		inp_ofrm_loop_tab = 0b00001;
		break;		//55
	case DPSS_FILM64:
		inp_ofrm_loop_tab = 0b0000010001;
		break;		//64
	case DPSS_FILM66:
		inp_ofrm_loop_tab = 0b000001;
		break;		//66
	case DPSS_FILM87:
		inp_ofrm_loop_tab = 0b000000010000001;
		break;		//87
	case DPSS_FILM212:
		inp_ofrm_loop_tab = 0b01011;
		break;		//212
	default:
		dbg_h2("<dpss.c>: film_mode ERROR\n");
		break;
	}

	inp_ofrm_vld = (inp_ofrm_loop_tab >> film_phs) & 0x1;//done
	config_inp_ofrm_flg(inp_ofrm_vld);

	dbg_h2("film_mode: %x film_phs: %x tab: %x inp_ofrm_vld: %x\n",
	       film_mode, film_phs, inp_ofrm_loop_tab, inp_ofrm_vld);

	if (badedit_en) {
		//inp_ofrm_vld = (inp_ofrm_loop_tab>>film_phs)&0x1;//done
		//config_inp_ofrm_flg(inp_ofrm_vld);
		// dbg_h2("<config_inp_loop_tab>: film_mode : %d film_phs : %d,
		//	inp_ofrm_vld: %d \n",film_mode,film_phs,inp_ofrm_vld);
	} else {
		wr(FRC_AUTO_MODE_LOOP_TAB, inp_ofrm_loop_tab);	//reg_inp_ofrm_loop_tab
		w_reg_bit(FRC_REG_FILM_PHS_1, film_phs, 24, 8);
		// dbg_h2("<config_inp_loop_tab>:film_mode:%d film_phs : %d,
			//inp_ofrm_vld: %d ,inp_ofrm_loop_tab:%08x,\n",
			//film_mode,film_phs,(inp_ofrm_loop_tab>>film_phs)&0x1,inp_ofrm_loop_tab);
	}
	return inp_ofrm_vld;
}

//cfg tbc
void config_dae_loop_tab(enum DPSS_FRC_RATIO frc_ratio, enum DPSS_FILM_MODE film_mode,
			 u32 cfg_at_inp_site)
{
//==============================================================//
// FILM_MODE
//==============================================================//
	u32 irn, irm, frn, frm, input_n, output_m;
	u32 inp_ofrm_step;
	//u32 force_film_mode = rd(FRC_REG_TOP_RESERVE1 );
	u32 dae_step_sel = (rd(FRC_REG_TOP_CTRL7) >> 20) & 0x3;

	dbg_h2("%s frc_ratio=%d, film_mode=%d, cfg_at_inp_site=%d\n", __func__,
	       frc_ratio, film_mode, cfg_at_inp_site);

	switch (frc_ratio) {
	case DPSS_FRC_RATIO_1_1:
		irn = 1;
		irm = 1;
		break;
	case DPSS_FRC_RATIO_1_2:
		irn = 1;
		irm = 2;
		break;
	case DPSS_FRC_RATIO_1_4:
		irn = 1;
		irm = 4;
		break;
	case DPSS_FRC_RATIO_1_5:
		irn = 1;
		irm = 5;
		break;
	case DPSS_FRC_RATIO_1_6:
		irn = 1;
		irm = 6;
		break;
	case DPSS_FRC_RATIO_2_5:
		irn = 2;
		irm = 5;
		break;
	case DPSS_FRC_RATIO_5_12:
		irn = 5;
		irm = 12;
		break;
	default:
		irn = 1;
		irm = 1;
		break;
	}

	switch (film_mode) {
	case DPSS_VIDEO:
		frn = 1;
		frm = 1;
		break;
	case DPSS_FILM22:
		frn = 1;
		frm = 2;
		break;
	case DPSS_FILM32:
		frn = 2;
		frm = 5;
		break;
	case DPSS_FILM3223:
		frn = 4;
		frm = 10;
		break;
	case DPSS_FILM2224:
		frn = 4;
		frm = 10;
		break;
	case DPSS_FILM32322:
		frn = 5;
		frm = 12;
		break;
	case DPSS_FILM44:
		frn = 1;
		frm = 4;
		break;
	case DPSS_FILM21111:
		frn = 5;
		frm = 6;
		break;
	case DPSS_FILM23322:
		frn = 5;
		frm = 12;
		break;
	case DPSS_FILM2111:
		frn = 4;
		frm = 5;
		break;
	case DPSS_FILM22224:
		frn = 5;
		frm = 12;
		break;
	case DPSS_FILM33:
		frn = 1;
		frm = 3;
		break;
	case DPSS_FILM334:
		frn = 3;
		frm = 10;
		break;
	case DPSS_FILM55:
		frn = 1;
		frm = 5;
		break;
	case DPSS_FILM64:
		frn = 2;
		frm = 10;
		break;
	case DPSS_FILM66:
		frn = 1;
		frm = 6;
		break;
	case DPSS_FILM87:
		frn = 2;
		frm = 15;
		break;
	case DPSS_FILM212:
		frn = 3;
		frm = 5;
		break;
	default:
		frn = 1;
		frm = 1;
		break;
	}
	input_n = irn * frn;
	output_m = irm * frm;
	//for C RUN,RTL don't need
	w_reg_bit(FRC_REG_PHS_TABLE, input_n, 24, 8);
	w_reg_bit(FRC_REG_PHS_TABLE, output_m, 16, 8);
	w_reg_bit(FRC_REG_OUT_FID, output_m, 24, 8);
	w_reg_bit(FRC_REG_PD_PAT_NUM, output_m, 0, 8);

	if (cfg_at_inp_site == 0 && dae_step_sel == 2) {
		inp_ofrm_step = ((input_n * 128 << 16) / output_m) >> 4;//;inp_ofrm_step_f>>16;
		config_dae_ofrm_step_at_dae_site(inp_ofrm_step);
		wr(FRC_AUTO_MODE_LOOP_CTRL, ((output_m & 0xff) << 8) |	//reg_inp_ofrm_loop_num
		   ((output_m & 0x1f) << 0));	//reg_dae_loop_step_num

		dbg_h2("dae_ofrm inp_ofrm_step=%d\n", inp_ofrm_step);
	} else if ((cfg_at_inp_site == 1) && (dae_step_sel == 1)) {
		inp_ofrm_step = ((input_n * 128 << 16) / output_m) >> 12;//;inp_ofrm_step_f>>16;
		dbg_h2("%s dae_step_sel=%d, inp_ofrm_step=%d\n", __func__,
		       dae_step_sel, inp_ofrm_step);
		config_dae_ofrm_step_at_inp_site(inp_ofrm_step);
		wr(FRC_AUTO_MODE_LOOP_CTRL, ((output_m & 0xff) << 8) |	//reg_inp_ofrm_loop_num
		   ((output_m & 0x1f) << 0));	//reg_dae_loop_step_num
	}
}

void config_inp_ofrm_flg(u32 inp_ofrm_vld)
{
	w_reg_bit(FRC_AUTO_MODE_INP_OFRM_WREN, inp_ofrm_vld, 0, 1);
}

void config_dae_ofrm_step_at_inp_site(u32 inp_ofrm_step)
{
	//dbg_h2("<config_dae_ofrm_step_at_inp_site>: dae_ofrm_step : %d\n",inp_ofrm_step);
	w_reg_bit(FRC_REG_TOP_CTRL8, inp_ofrm_step, 0, 12);
}

void config_dae_ofrm_step_at_dae_site(u32 dae_ofrm_step)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	//dbg_h2("<config_dae_ofrm_step_at_dae_site>: dae_ofrm_step : %d\n",dae_ofrm_step);
	if (prm_top->fw_en == 0)
		w_reg_bit(FRC_REG_TOP_CTRL7, dae_ofrm_step, 0, 20);
}

