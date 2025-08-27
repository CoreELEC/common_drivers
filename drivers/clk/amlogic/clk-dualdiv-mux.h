/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_CLK_DUALDIV_MUX_H
#define __AML_CLK_DUALDIV_MUX_H

#include <linux/clk-provider.h>

struct aml_clk_dualdiv_param {
	unsigned int div_mode;
	unsigned int n0;
	unsigned int m0;
	unsigned int n1;
	unsigned int m1;
};

struct aml_clk_dualdiv_data {
	unsigned int			reg_offset;
	struct	aml_clk_dualdiv_param	*table;
	unsigned int			table_count;
};

struct aml_clk_dualdiv_mux_data {
	unsigned int	reg_offset;
	u32		*table;
};

extern const struct clk_ops aml_clk_dualdiv_ops;
extern const struct clk_ops aml_clk_dualdiv_ro_ops;
extern const struct clk_ops aml_clk_dualdiv_mux_ops;
extern const struct clk_ops aml_clk_dualdiv_mux_ro_ops;

#ifdef CONFIG_AMLOGIC_MODIFY
int clk_dualdiv_mux_init(void);
void clk_dualdiv_mux_exit(void);
#endif  /* CONFIG_AMLOGIC_MODIFY */

#endif  /* __AML_CLK_DUALDIV_MUX_H */
