/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_CLK_GENERAL_H
#define __AML_CLK_GENERAL_H

#include <linux/clk-provider.h>

struct aml_clk_general_data {
	unsigned int	reg_offset;
	u8		bit_offset;
	u8		div_width;
	u32		*mux_table;
};

extern const struct clk_ops aml_clk_general_ops;

#ifdef CONFIG_AMLOGIC_MODIFY
int clk_general_init(void);
void clk_general_exit(void);
#endif  /* CONFIG_AMLOGIC_MODIFY */

#endif /* __AML_CLK_GENERAL_H */
