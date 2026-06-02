// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#endif
#include "d_define.h"
#include "d_pq_mgr.h"
#include "dpss_pq_drv.h"

#ifdef CONFIG_AMLOGIC_MEDIA_DPSS
int di_pq_mgr_demosquito_param(struct di_demosquito_param_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return ret;
	ret = di_set_demosquito_reg(pdata);
	return ret;
}
EXPORT_SYMBOL(di_pq_mgr_demosquito_param);

int di_pq_mgr_dnr_param(struct di_dnr_param_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return ret;
	ret = di_set_dnr_reg(pdata);
	return ret;
}
EXPORT_SYMBOL(di_pq_mgr_dnr_param);

int di_pq_mgr_dct_param(struct di_dct_param_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return ret;
	ret = di_set_dct_reg(pdata);
	return ret;
}
EXPORT_SYMBOL(di_pq_mgr_dct_param);

int di_pq_mgr_dblk_param(struct di_dblk_param_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return ret;
	ret = di_set_dblk_reg(pdata);
	return ret;
}
EXPORT_SYMBOL(di_pq_mgr_dblk_param);

int di_pq_mgr_xlr_param(struct di_xlr_param_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return ret;
	ret = di_set_xlr_reg(pdata);
	return ret;
}
EXPORT_SYMBOL(di_pq_mgr_xlr_param);
#endif

