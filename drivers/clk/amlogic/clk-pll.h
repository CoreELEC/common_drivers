/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_CLK_PLL_H
#define __AML_CLK_PLL_H

#include <linux/clk-provider.h>
#include <linux/regmap.h>

/* Amlogic PLL special parameters */
#define AML_PLL_SPEC_PARM_N		BIT(0)
#define AML_PLL_SPEC_PARM_FRAC		BIT(1)
#define AML_PLL_SPEC_PARM_OD		BIT(2)
#define AML_PLL_SPEC_PARM_LOCK_DETECT	BIT(3)

struct aml_pll_parms_table {
	unsigned int	m;
	unsigned int	n;
	unsigned int	od;
};

struct aml_pll_dco_range {
	unsigned long long	min;
	unsigned long long	max;
};

#define AML_PLL_ROUND_CLOSEST		BIT(0)
#define AML_PLL_M_EN0P5			BIT(1)
#define AML_PLL_READ_ONLY		BIT(2)
#define AML_PLL_FORCE_INIT		BIT(3)

struct aml_pll_data {
	struct reg_sequence		*init_regs;
	unsigned int			init_count;
	struct aml_pll_parms_table	*table;
	unsigned int			table_count;
	struct aml_pll_dco_range	range;
	unsigned int			frac_max;
	u8				od_max;
	u16				parm_flags;
	u16				flags;
	/* Save the context information of the clock */
	int				context_is_enabled;
	unsigned long			context_rate;
};

extern const struct clk_ops aml_pll_ops;
extern const struct clk_ops aml_pll_ro_ops;

#ifdef CONFIG_AMLOGIC_MODIFY
int clk_pll_init(void);
void clk_pll_exit(void);
#endif  /* CONFIG_AMLOGIC_MODIFY */

#endif  /* __AML_CLK_PLL_H */
