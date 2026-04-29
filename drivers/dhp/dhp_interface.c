// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/dhp_interface.h>

static AMLOGIC_DHP_PROCESS_fun_t g_dhp_fun;

int AMLOGIC_DHP_PROCESS(struct dhp_output *out,
						struct dhp_input *in,
						u32 flag)

{
	if (g_dhp_fun) {
		return g_dhp_fun(out,
			in,
			flag);
	}
	pr_err("no %s!!\n", __func__);
	return -1;
}
EXPORT_SYMBOL(AMLOGIC_DHP_PROCESS);

int register_amlogic_dhp_fun(AMLOGIC_DHP_PROCESS_fun_t fn)
{
	if (g_dhp_fun) {
		pr_err("error!!,AMLOGIC_DHP have register\n");
		return -1;
	}

	g_dhp_fun = fn;
	return 0;
}
EXPORT_SYMBOL(register_amlogic_dhp_fun);

int unregister_amlogic_dhp_fun(void)
{
	g_dhp_fun = NULL;

	return 0;
}
EXPORT_SYMBOL(unregister_amlogic_dhp_fun);

static int __init dhp_init(void)
{
	return 0;
}

static void __exit dhp_exit(void)
{
}

module_init(dhp_init);
module_exit(dhp_exit);
MODULE_LICENSE("GPL v2");
