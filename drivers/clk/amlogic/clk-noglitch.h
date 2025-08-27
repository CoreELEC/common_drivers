/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 Amlogic, inc.
 * Author: Chuan Liu <chuan.liu@amlogic.com>
 */

#ifndef __AML_CLK_NOGLITCH_H
#define __AML_CLK_NOGLITCH_H

#include <linux/clk-provider.h>

struct aml_clk_noglitch_data {
	unsigned int	reg_offset;
	u32		*mux_table;
};

extern const struct clk_ops aml_clk_noglitch_ops;

#ifdef CONFIG_AMLOGIC_MODIFY
int clk_noglitch_init(void);
void clk_noglitch_exit(void);
#endif  /* CONFIG_AMLOGIC_MODIFY */

#endif  /* __AML_CLK_NOGLITCH_H */
