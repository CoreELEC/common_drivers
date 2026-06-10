// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>

#include <linux/compat.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/amlogic/pm.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/aml_free_reserved.h>

#include <linux/page-flags.h>
#include <linux/mm.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "panel_utils.h"
#include <linux/amlogic/media/vout/panel_utils/json_parse.h>
#include <linux/amlogic/media/vout/panel_utils/lcd_resman.h>
#include <linux/amlogic/media/vout/panel_utils/aml_model.h>

struct json_parse_s *get_panel_jsp(int index)
{
	if (get_lcd_panel_file_type(index) != PANEL_FILE_JSON)
		return NULL;

	return (struct json_parse_s *)get_panel_file_parse_mem(index);
}

int lcd_panel_json_init_try_mem(int index, void *mem)
{
#define JSON_PANEL_HANDLE_HEAD_SIZE (32)
	struct json_parse_s *jsp;
	struct json_panel_handle_head_s {
		unsigned int size;
		unsigned int json_cnt;
		unsigned int js_len;
		unsigned int json_start;
		unsigned int js_start;
		unsigned char rsvd[JSON_PANEL_HANDLE_HEAD_SIZE - 20];
	} *head; //parse memory handled from uboot

	if (!mem)
		return -1;

	jsp = kzalloc(sizeof(*jsp), GFP_KERNEL);
	if (!jsp)
		return -1;
	head = (struct json_panel_handle_head_s *)mem;

	jsp->json_cnt =  head->json_cnt;
	jsp->js_len   = head->js_len;
	jsp->js       = kzalloc(jsp->js_len, GFP_KERNEL);
	jsp->root     = kcalloc(jsp->json_cnt, sizeof(*jsp->root), GFP_KERNEL);
	jsp->json_max = jsp->json_cnt;
	jsp->js_max = jsp->js_len;
	if (!jsp->js || !jsp->root) {
		json_deinit(jsp);
		kfree(jsp);
		return -1;
	}
	memcpy(jsp->root, mem + head->json_start, jsp->json_cnt * sizeof(*jsp->root));
	memcpy(jsp->js, mem + head->js_start, jsp->js_len);
	jsp->status = JSON_STATUS_OK;

	set_lcd_panel_file_type(index, PANEL_FILE_JSON);
	set_panel_file_parse_mem(index, (void *)jsp, sizeof(*jsp), PANEL_FILE_JSON);

	return 0;
}

int lcd_panel_json_init_try_file(int index, void *mem, unsigned int fsize)
{
	struct json_parse_s *jsp;

	if (!mem)
		return -1;

	jsp = kzalloc(sizeof(*jsp), GFP_KERNEL);
	if (!jsp)
		return -1;

	if (json_init(jsp, JSON_STR_MAX, JSON_NODE_MAX) < 0) {
		kfree(jsp->js);
		kfree(jsp->root);
		kfree(jsp);
		return -1;
	}
	if (!json_parse(jsp, (char *)mem, fsize)) {
		json_deinit(jsp);
		kfree(jsp);
		return -1;
	}
	jsp->status = JSON_STATUS_OK;

	set_lcd_panel_file_type(index, PANEL_FILE_JSON);
	set_panel_file_parse_mem(index, (void *)jsp, sizeof(*jsp), PANEL_FILE_JSON);
	return 0;
}

void panel_json_mem_free(void *mem)
{
	struct json_parse_s *jsp;

	if (!mem)
		return;

	jsp = (struct json_parse_s *)mem;
	if (!json_parse_ok(jsp)) {
		MODEL_PR("%s: not json type", __func__);
		kfree(mem);
		return;
	}

	json_deinit(jsp);
	kfree(jsp);
}

void panel_json_mem_dump(void *mem)
{
	struct json_parse_s *jsp;

	if (!mem)
		return;

	jsp = (struct json_parse_s *)mem;
	if (!json_parse_ok(jsp))
		return;

	json_dump_path(jsp, NULL);
}
