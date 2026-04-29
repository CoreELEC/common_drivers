// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/avbc_wrapper_interface.h>

static AMLOGIC_AVBC_WRAPPER_vframe_decoder_fun_t g_avbc_fun;

int AMLOGIC_AVBC_WRAPPER_vframe_decoder(struct avbc_output *out,
							struct avbc_input *in,
							u32 flag)

{
	if (g_avbc_fun) {
		return g_avbc_fun(out,
			in,
			flag);
	}
	pr_err("no %s ERRR!!\n", __func__);
	return -1;
}
EXPORT_SYMBOL(AMLOGIC_AVBC_WRAPPER_vframe_decoder);

int register_amlogic_avbc_wrapper_fun(AMLOGIC_AVBC_WRAPPER_vframe_decoder_fun_t fn)
{
	if (g_avbc_fun) {
		pr_err("error!!,AMLOGIC_AVBC_WRAPPER have register\n");
		return -1;
	}

	g_avbc_fun = fn;
	return 0;
}
EXPORT_SYMBOL(register_amlogic_avbc_wrapper_fun);

int unregister_amlogic_avbc_wrapper_fun(void)
{
	g_avbc_fun = NULL;

	return 0;
}
EXPORT_SYMBOL(unregister_amlogic_avbc_wrapper_fun);

