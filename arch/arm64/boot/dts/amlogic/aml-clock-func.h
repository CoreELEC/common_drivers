/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_CLOCK_FUNC_H
#define __AML_CLOCK_FUNC_H

/**
 * flags used across common struct clk. these flags should only affect the
 * top-level framework.
 *
 * Please corresponding updates when clk-provider.h flags are modified!!!
 */
#define CLK_SET_RATE_GATE		0x1
#define CLK_SET_PARENT_GATE		0x2
#define CLK_SET_RATE_PARENT		0x4
#define CLK_IGNORE_UNUSED		0x8

#define CLK_GET_RATE_NOCACHE		0x40
#define CLK_SET_RATE_NO_REPARENT	0x80
#define CLK_GET_ACCURACY_NOCACHE	0x100
#define CLK_RECALC_NEW_RATES		0x200
#define CLK_SET_RATE_UNGATE		0x400
#define CLK_IS_CRITICAL			0x800
#define CLK_OPS_PARENT_ENABLE		0x1000
#define CLK_DUTY_CYCLE_PARENT		0x2000

/**
 * hardware-specific flags of gate.
 *
 * Please corresponding updates when clk-provider.h flags are modified!!!
 */
#define CLK_GATE_SET_TO_DISABLE		0x1
#define CLK_GATE_HIWORD_MASK		0x2
#define CLK_GATE_BIG_ENDIAN		0x4

/**
 * hardware-specific flags of divider.
 *
 * Please corresponding updates when clk-provider.h flags are modified!!!
 */
#define CLK_DIVIDER_ONE_BASED		0x1
#define CLK_DIVIDER_POWER_OF_TWO	0x2
#define CLK_DIVIDER_ALLOW_ZERO		0x4
#define CLK_DIVIDER_HIWORD_MASK		0x8
#define CLK_DIVIDER_ROUND_CLOSEST	0x10
#define CLK_DIVIDER_READ_ONLY		0x20
#define CLK_DIVIDER_MAX_AT_ZERO		0x40
#define CLK_DIVIDER_BIG_ENDIAN		0x80
#define CLK_DIVIDER_EVEN_INTEGERS	0x100

/**
 * hardware-specific flags of mux.
 *
 * Please corresponding updates when clk-provider.h flags are modified!!!
 */
#define CLK_MUX_INDEX_ONE		0x1
#define CLK_MUX_INDEX_BIT		0x2
#define CLK_MUX_HIWORD_MASK		0x4
#define CLK_MUX_READ_ONLY		0x8
#define CLK_MUX_ROUND_CLOSEST		0x10
#define CLK_MUX_BIG_ENDIAN		0x20

/**
 * Amlogic basic clock types.
 */
#define AML_CLKTYPE_MUX			1
#define AML_CLKTYPE_DIV			2
#define AML_CLKTYPE_GATE		3

/**
 * Amlogic PLL special parameters.
 *
 * Please corresponding updates when drivers/clk/amlogic/clk-pll.h flags are
 * modified!!!
 */
#define AML_PLL_SPEC_PARM_N		0x1
#define AML_PLL_SPEC_PARM_FRAC		0x2
#define AML_PLL_SPEC_PARM_OD		0x4
#define AML_PLL_SPEC_PARM_LOCK_DETECT	0x8
#define AML_PLL_SPEC_PARM_EN0P5		0x10

/**
 * hardware-specific flags of Amlogic PLL.
 *
 * Please corresponding updates when drivers/clk/amlogic/clk-pll.h flags are
 * modified!!!
 */
#define AML_PLL_ROUND_CLOSEST		0x1
#define AML_PLL_M_EN0P5			0x2
#define AML_PLL_READ_ONLY		0x4
#define AML_PLL_FORCE_INIT		0x8

#endif  /* __AML_CLOCK_FUNC_H */
