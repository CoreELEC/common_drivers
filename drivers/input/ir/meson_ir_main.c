// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/amlogic/pm.h>
#include "meson_ir_main.h"
#include <linux/amlogic/gki_module.h>
#include <linux/mailbox_controller.h>
#include <linux/amlogic/aml_mbox.h>
#include <linux/mailbox_client.h>

static atomic_t meson_ir_dev_no = ATOMIC_INIT(-1);

static int disable_ir;
static int get_irenv(char *str)
{
	int ret;

	ret = kstrtouint(str, 10, &disable_ir);
	if (ret)
		return -EINVAL;
	return 1;
}

__setup("disable_ir=", get_irenv);

int meson_ir_read_dev_num(void)
{
	return meson_ir_dev_no.counter + 1;
}

int meson_ir_mbox_transfer(struct meson_ir_chip *chip, u32 *data, u32 size)
{
	int ret;

	if (IS_ERR_OR_NULL(chip->mbox_chan))
		return 0;

	ret = aml_mbox_transfer_data(chip->mbox_chan, MBOX_CMD_SET_IR_STATE,
				     data, size, data, size, MBOX_SYNC);
	if (ret < 0)
		dev_err(chip->dev, "Fail to send mbox message\n");

	return ret;
}

void meson_ir_mbox_set_wakeup_key(struct meson_ir_chip *chip, u32 key_num)
{
	int cnt, i;
	u32 data[IR_MBOX_BUF_MAX_KEY + 3] = {IR_MBOX_CMD_SET_WAKEUP_LIST};

	/* 3 items for cmd id, key num, ir wakeup reason */
	for (i = 0, cnt = 0; i < key_num; i++) {
		data[2] |= (chip->wakeup_tab[i].ir_reason & 0x1) << cnt;
		data[cnt + 3] = chip->wakeup_tab[i].frame_code;
		cnt++;

		if (cnt != IR_MBOX_BUF_MAX_KEY)
			continue;

		cnt = 0;
		data[1] = 16;
		meson_ir_mbox_transfer(chip, data, sizeof(data));
		data[2] = 0;
	}
	data[1] = cnt;
	meson_ir_mbox_transfer(chip, data, sizeof(data));
}

void meson_ir_report_wakeup_event(struct meson_ir_chip *chip, u32 framecode)
{
	struct input_dev *input_device = NULL;
	unsigned long flags;
	int i = 0;
	unsigned int keycode = 0;
	struct ir_wakeup_tab *wt = chip->wakeup_tab;

	if (IS_ERR_OR_NULL(chip->mbox_chan)) {
		if (get_resume_method() == REMOTE_WAKEUP)
			keycode = KEY_POWER;
		else if (get_resume_method() == REMOTE_CUS_WAKEUP)
			keycode = KEY_COPY;

		goto input_report;
	}

	if (get_resume_method() != REMOTE_WAKEUP)
		return;

	while (wt[i].frame_code) {
		if (wt[i].frame_code == framecode)
			break;
		i++;
	}

	if ((wt[i].report_val == KEY_POWER && chip->r_dev->is_probed) ||
	    wt[i].report_val == 0)
		return;

	keycode = wt[i].report_val;
	chip->r_dev->cur_hardcode = framecode;
	chip->r_dev->is_valid_custom(chip->r_dev);

	if (chip->cur_tab)
		input_device = meson_ir_match_input_dev(chip->r_dev,
							chip->cur_tab);
input_report:
	if (!input_device) {
		input_device = chip->r_dev->input_devs[0];
		dev_err(chip->dev, "wakeup framecode = 0x%x\n", framecode);
	}

	spin_lock_irqsave(&chip->slock, flags);

	input_event(input_device, EV_KEY, keycode, 1);
	input_sync(input_device);
	input_event(input_device, EV_KEY, keycode, 0);
	input_sync(input_device);

	spin_unlock_irqrestore(&chip->slock, flags);
}

void meson_ir_set_irq(struct meson_ir_chip *chip, int enable)
{
	if (enable) {
		enable_irq(chip->irqno[0]);
		if (chip->irqno[1] >= 0)
			enable_irq(chip->irqno[1]);
	} else {
		disable_irq(chip->irqno[0]);
		if (chip->irqno[1] >= 0)
			disable_irq(chip->irqno[1]);
	}
}

int meson_ir_pulses_malloc(struct meson_ir_chip *chip)
{
	struct meson_ir_dev *r_dev = chip->r_dev;
	int len = r_dev->max_learned_pulse;
	int ret = 0;

	if (r_dev->pulses) {
		dev_info(chip->dev, "ir learning pulse already exists\n");
		return ret;
	}

	r_dev->pulses = kzalloc(sizeof(*r_dev->pulses) +
				len * sizeof(unsigned int), GFP_KERNEL);

	if (!r_dev->pulses) {
		dev_err(chip->dev, "ir learning pulse alloc err\n");
		ret = -ENOMEM;
	}

	return ret;
}

void meson_ir_pulses_free(struct meson_ir_chip *chip)
{
	struct meson_ir_dev *r_dev = chip->r_dev;

	kfree(r_dev->pulses);
	r_dev->pulses = NULL;
}

int meson_ir_scancode_sort(struct ir_map_tab *ir_map)
{
	bool is_sorted;
	u32 tmp;
	int i;
	int j;

	for (i = 0; i < ir_map->map_size - 1; i++) {
		is_sorted = true;
		for (j = 0; j < ir_map->map_size - i - 1; j++) {
			if (ir_map->codemap[j].map.scancode >
			   ir_map->codemap[j + 1].map.scancode) {
				is_sorted = false;
				tmp = ir_map->codemap[j].code;
				ir_map->codemap[j].code  =
						ir_map->codemap[j + 1].code;
				ir_map->codemap[j + 1].code = tmp;
			}
		}
		if (is_sorted)
			break;
	}

	return 0;
}

struct meson_ir_map_tab_list *meson_ir_seek_map_tab(struct meson_ir_chip *chip,
						    unsigned int custom_code)
{
	struct meson_ir_map_tab_list *ir_map = NULL;

	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		if (ir_map->tab.custom_code == custom_code)
			return ir_map;
	}
	return NULL;
}

void meson_ir_map_tab_list_free(struct meson_ir_chip *chip)
{
	struct meson_ir_map_tab_list *ir_map, *t_map;

	list_for_each_entry_safe(ir_map, t_map, &chip->map_tab_head, list) {
		list_del(&ir_map->list);
		kfree((void *)ir_map);
	}
}

void meson_ir_tab_free(struct meson_ir_map_tab_list *ir_map_list)
{
	kfree((void *)ir_map_list);
	ir_map_list = NULL;
}

static int meson_ir_lookup_by_scancode(struct ir_map_tab *ir_map,
				       unsigned int scancode)
{
	int start = 0;
	int end = ir_map->map_size - 1;
	int mid;

	while (start <= end) {
		mid = (start + end) >> 1;
		if (ir_map->codemap[mid].map.scancode < scancode)
			start = mid + 1;
		else if (ir_map->codemap[mid].map.scancode > scancode)
			end = mid - 1;
		else
			return mid;
	}

	return -ENODATA;
}

static int meson_ir_report_rel(struct meson_ir_dev *dev, u32 scancode,
			       int status)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *ct = chip->cur_tab;
	static u32 repeat_count;
	s32 cursor_value = 0;
	u32 valid_scancode;
	u16 mouse_code;
	struct input_dev *input_device;
	s32 move_accelerate[] = CURSOR_MOVE_ACCELERATE;

	/*nothing need to do in normal mode*/
	if (!ct || ct->ir_dev_mode != MOUSE_MODE)
		return -EINVAL;

	if (status == IR_STATUS_REPEAT) {
		valid_scancode = dev->last_scancode;
		repeat_count++;
		if (repeat_count > ARRAY_SIZE(move_accelerate) - 1)
			repeat_count = ARRAY_SIZE(move_accelerate) - 1;
	} else {
		valid_scancode = scancode;
		dev->last_scancode = scancode;
		repeat_count = 0;
	}
	if (valid_scancode == ct->tab.cursor_code.cursor_left_scancode) {
		cursor_value = -(1 + move_accelerate[repeat_count]);
		mouse_code = REL_X;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_right_scancode) {
		cursor_value = 1 + move_accelerate[repeat_count];
		mouse_code = REL_X;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_up_scancode) {
		cursor_value = -(1 + move_accelerate[repeat_count]);
		mouse_code = REL_Y;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_down_scancode) {
		cursor_value = 1 + move_accelerate[repeat_count];
		mouse_code = REL_Y;
	} else {
		return -EINVAL;
	}

	input_device = meson_ir_match_input_dev(dev, ct);
	if (!input_device)
		return 0;

	input_event(input_device, EV_REL, mouse_code, cursor_value);
	input_sync(input_device);

	meson_ir_dbg(chip->r_dev, "mouse cursor be %s moved %d.\n",
		     mouse_code == REL_X ? "horizontal" : "vertical",
		     cursor_value);

	return 0;
}

static u32 meson_ir_getkeycode(struct meson_ir_dev *dev, u32 scancode)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *ct = chip->cur_tab;
	int cus_code;
	int index;

	if (!ct) {
		dev_err(chip->dev, "cur_custom is null\n");
		return KEY_RESERVED;
	}
	/*return BTN_LEFT in mouse mode*/
	if (ct->ir_dev_mode == MOUSE_MODE &&
	    scancode == ct->tab.cursor_code.cursor_ok_scancode) {
		meson_ir_dbg(chip->r_dev, "mouse left button scancode: 0x%x",
			     BTN_LEFT);
		return BTN_LEFT;
	}

lookup_again:
	index = meson_ir_lookup_by_scancode(&ct->tab, scancode);
	if (index < 0) {
		cus_code = chip->ir_contr[chip->ir_work].get_custom_code(chip);
		list_for_each_entry_continue(ct, &chip->map_tab_head, list) {
			if (ct->tab.custom_code == cus_code) {
				chip->cur_tab = ct;
				dev->keyup_delay = ct->tab.release_delay;
				goto lookup_again;
			}
		}
		dev_err(chip->dev, "scancode %d undefined\n", scancode);
		return KEY_RESERVED;
	}

	/*save IR remote-control work mode*/
	if (!dev->keypressed &&
	    scancode == ct->tab.cursor_code.fn_key_scancode) {
		if (ct->ir_dev_mode == NORMAL_MODE) {
			ct->ir_dev_mode = MOUSE_MODE;
		} else {
			ct->ir_dev_mode = NORMAL_MODE;
		}
		dev_info(chip->dev, "IR remote control[ID: 0x%x] switch to %s\n",
			 ct->tab.custom_code, ct->ir_dev_mode ?
			 "mouse mode" : "normal mode");
	}

	return ct->tab.codemap[index].map.keycode;
}

static bool meson_ir_is_valid_custom(struct meson_ir_dev *dev)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *old_ct = chip->cur_tab;
	int custom_code;

	if (!chip->ir_contr[chip->ir_work].get_custom_code)
		return true;
	custom_code = chip->ir_contr[chip->ir_work].get_custom_code(chip);
	chip->cur_tab = meson_ir_seek_map_tab(chip, custom_code);
	if (chip->cur_tab) {
		if (chip->cur_tab->tab.repeat_enable)
			dev->keyup_delay = chip->cur_tab->tab.repeat_delay;
		else
			dev->keyup_delay = chip->cur_tab->tab.release_delay;
		return true;
	}
	chip->cur_tab = old_ct;
	return false;
}

static bool meson_ir_is_next_repeat(struct meson_ir_dev *dev)
{
	unsigned int val = 0;
	unsigned char fbusy = 0;

	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;

	regmap_read(chip->ir_contr[chip->ir_work].base, REG_STATUS, &val);
	fbusy |= IR_CONTROLLER_BUSY(val);

	meson_ir_dbg(chip->r_dev, "ir controller busy flag = %d\n", fbusy);
	if (!dev->wait_next_repeat && fbusy)
		return true;
	else
		return false;
}

static bool meson_ir_set_custom_code(struct meson_ir_dev *dev, u32 code)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;

	return chip->ir_contr[chip->ir_work].set_custom_code(chip, code);
}

static void meson_ir_tasklet(struct tasklet_struct *t)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)t->data;
	struct input_dev *input_device;
	unsigned long flags;
	int status = -1;
	int scancode = -1;

	/**
	 *need first get_scancode, then get_decode_status, the status
	 *may was set flag from get_scancode function
	 */
	spin_lock_irqsave(&chip->slock, flags);
	if (chip->ir_contr[chip->ir_work].get_scancode)
		scancode = chip->ir_contr[chip->ir_work].get_scancode(chip);
	if (chip->ir_contr[chip->ir_work].get_decode_status)
		status = chip->ir_contr[chip->ir_work].get_decode_status(chip);

	switch (status) {
	case IR_STATUS_NORMAL:
		meson_ir_dbg(chip->r_dev, "receive scancode=0x%x\n", scancode);
		meson_ir_keydown(chip->r_dev, scancode, status);
		break;
	case IR_STATUS_REPEAT:
		meson_ir_dbg(chip->r_dev, "receive repeat\n");
		meson_ir_keydown(chip->r_dev, scancode, status);
		break;
	case IR_STATUS_CUSTOM_DATA:
	case IR_STATUS_CHECKSUM_ERROR:
		dev_err(chip->dev, "decoder error %d\n", status);
		break;
	default:
		dev_err(chip->dev, "receive error %d\n", status);
		break;
	}
	spin_unlock_irqrestore(&chip->slock, flags);

	if (chip->cur_tab && chip->cur_tab->ir_dev_mode == MOUSE_MODE) {
		input_device = meson_ir_match_input_dev(chip->r_dev,
							chip->cur_tab);
		if (input_device && !test_bit(EV_REL, input_device->evbit))
			queue_work(system_wq, &chip->ir_workqueue);
	}
}

static void meson_ir_input_mouse_configure(struct meson_ir_dev *dev)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *ct = chip->cur_tab;
	struct input_dev *new_input, *old_input = NULL;
	int ret, i, j;

	if (!ct)
		return;

	for (i = 0; i < dev->input_dev_num; i++)
		if (!memcmp(&ct->tab.id, &dev->input_devs[i]->id,
			    sizeof(struct input_id))) {
			old_input = dev->input_devs[i];
			break;
		}

	if (!old_input)
		return;
	if (test_bit(EV_REL, old_input->evbit))
		return;

	new_input = devm_input_allocate_device(chip->dev);
	if (!new_input)
		return;

	new_input->name = old_input->name;
	new_input->phys = "keypad/input0";
	new_input->dev.parent = old_input->dev.parent;
	memcpy(&new_input->id, &old_input->id, sizeof(struct input_id));
	new_input->rep[REP_DELAY] = 0xffffffff;  /*close input repeat*/
	new_input->rep[REP_PERIOD] = 0xffffffff; /*close input repeat*/

	bitmap_copy(new_input->keybit, old_input->keybit, KEY_MAX);
	bitmap_copy(new_input->evbit, old_input->evbit, EV_MAX);
	input_set_capability(new_input, EV_REL, REL_X);
	input_set_capability(new_input, EV_REL, REL_Y);
	input_set_capability(new_input, EV_REL, REL_WHEEL);
	for (j = BTN_MOUSE; j < BTN_SIDE; j++)
		input_set_capability(new_input, EV_KEY, j);

	input_set_drvdata(new_input, dev);

	input_unregister_device(old_input);
	ret = input_register_device(new_input);
	if (ret < 0) {
		dev_err(chip->dev, "re-register input failed\n");
		return;
	}

	dev->input_devs[i] = new_input;
}

static void meson_ir_workqueue(struct work_struct *work)
{
	struct meson_ir_chip *chip = container_of(work, struct meson_ir_chip,
						  ir_workqueue);

	meson_ir_input_mouse_configure(chip->r_dev);
}

static void meson_ir_get_fifo_work(struct work_struct *work)
{
	struct meson_ir_chip *chip = container_of(work, struct meson_ir_chip,
						  get_fifo_workqueue);
	struct meson_ir_dev *r_dev = chip->r_dev;
	int val;
	int is_fifo_pending, is_fifo_timeout, is_fifo_empty;
	enum raw_event_type type = RAW_SPACE;

	regmap_read(chip->ir_contr[chip->ir_work].base, REG_FIFO_CTL, &val);
	is_fifo_pending = (val >> 30) & 0x01;
	is_fifo_timeout = (val >> 29) & 0x01;
	is_fifo_empty = (val >> 27) & 0x01;

	/*disable  interrupt*/
	regmap_update_bits(chip->ir_contr[chip->ir_work].base, REG_FIFO_CTL,
			   GENMASK(23, 22), 0);

	if (is_fifo_pending || is_fifo_timeout) {
		/*clear state*/
		regmap_update_bits(chip->ir_contr[chip->ir_work].base,
				REG_FIFO_CTL, GENMASK(30, 29), GENMASK(30, 29));

		for (; !is_fifo_empty; ) {
			regmap_read(chip->ir_contr[chip->ir_work].base,
				    REG_WIDTH_NEW, &val);
			val = val & GENMASK(12, 0);

			if (r_dev->ir_learning_on) {
				r_dev->pulses->pulse[r_dev->pulses->len] = val;
				r_dev->pulses->pulse[r_dev->pulses->len] |=
						(r_dev->pulses->len % 2) << 31;

				r_dev->pulses->len++;
			} else if (MULTI_IR_SOFTWARE_DECODE(chip->protocol)) {
				val = val * 10 * 1000;
				type = RAW_PULSE;
				meson_ir_raw_event_store_edge(r_dev, type, val);
				meson_ir_raw_event_handle(r_dev);
			}

			regmap_read(chip->ir_contr[chip->ir_work].base,
				    REG_FIFO_CTL, &val);
			is_fifo_empty = (val >> 27) & 0x01;
		}
	}

	/*enable interrupt*/
	regmap_update_bits(chip->ir_contr[chip->ir_work].base, REG_FIFO_CTL,
			   GENMASK(23, 22), GENMASK(23, 22));
}

static irqreturn_t meson_ir_interrupt(int irq, void *dev_id)
{
	struct meson_ir_chip *rc = (struct meson_ir_chip *)dev_id;
	struct meson_ir_dev *r_dev = rc->r_dev;
	int contr_status = 0;
	int val = 0;
	u32 duration;
	unsigned char cnt;
	enum raw_event_type type = RAW_SPACE;

	if (!r_dev->enable)
		return IRQ_HANDLED;

	regmap_read(rc->ir_contr[MULTI_IR_ID].base, REG_REG1, &val);
	val = (val & GENMASK(28, 16)) >> 16;

	if (r_dev->ir_learning_on) {
		if (r_dev->pulses->len >= r_dev->max_learned_pulse)
			return IRQ_HANDLED;

		if (r_dev->use_fifo) {
			queue_work(system_wq, &rc->get_fifo_workqueue);
			return IRQ_HANDLED;
		}

		/*get pulse durations*/
		r_dev->pulses->pulse[r_dev->pulses->len] = val & GENMASK(30, 0);

		/*get pulse level*/
		regmap_read(rc->ir_contr[MULTI_IR_ID].base, REG_STATUS, &val);
		val = !!((val >> 8) & BIT(1));
		r_dev->pulses->pulse[r_dev->pulses->len] &= ~BIT(31);

		if (val)
			r_dev->pulses->pulse[r_dev->pulses->len] |= BIT(31);
		r_dev->pulses->len++;

		return IRQ_HANDLED;
	}

	/**
	 *software decode multiple protocols by using Time Measurement of
	 *multi-format IR controller
	 */
	if (MULTI_IR_SOFTWARE_DECODE(rc->protocol)) {
		if (r_dev->ir_learning_on) {
			queue_work(system_wq, &rc->get_fifo_workqueue);
			return IRQ_HANDLED;
		}

		rc->ir_work = MULTI_IR_ID;
		duration = val * 10 * 1000;
		type = RAW_PULSE;
		meson_ir_raw_event_store_edge(rc->r_dev, type, duration);
		meson_ir_raw_event_handle(rc->r_dev);
	} else {
		for (cnt = 0; cnt < (ENABLE_LEGACY_IR(rc->protocol)
		     ? 2 : 1); cnt++) {
			regmap_read(rc->ir_contr[cnt].base, REG_STATUS,
				    &contr_status);
			if (IR_DATA_IS_VALID(contr_status)) {
				rc->ir_work = cnt;
				tasklet_schedule(&rc->tasklet);
				return IRQ_HANDLED;
			}
		}

		if (cnt == (ENABLE_LEGACY_IR(rc->protocol) ? 2 : 1)) {
			dev_err(rc->dev, "invalid interrupt.\n");
			return IRQ_HANDLED;
		}
	}
	return IRQ_HANDLED;
}

static int meson_ir_get_wakeup_tables(struct device_node *node,
				      struct meson_ir_chip *chip)
{
	const phandle *phandle;
	struct device_node *wakeup_maps;
	unsigned int key_num;
	int ret = -1;

	phandle = of_get_property(node, "wakeup_map", NULL);
	if (!phandle) {
		dev_err(chip->dev, "can't find wakeup map\n");
		return -EINVAL;
	}

	wakeup_maps = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!wakeup_maps) {
		dev_err(chip->dev, "can't find device node key\n");
		return -ENODATA;
	}

	ret = of_property_read_u32(wakeup_maps, "key_num", &key_num);
	if (ret) {
		dev_err(chip->dev, "please config correct wakeup key num\n");
		return -ENODATA;
	}

	if (key_num > MAX_WAKEUP_KEY)
		key_num = MAX_WAKEUP_KEY;

	chip->wakeup_tab = kcalloc((MAX_WAKEUP_KEY + 1),
				   sizeof(*chip->wakeup_tab), GFP_KERNEL);
	if (!chip->wakeup_tab)
		return -ENOMEM;

	ret = of_property_read_u32_array(wakeup_maps, "wakeup_key",
					 (u32 *)chip->wakeup_tab,
			     key_num * sizeof(*chip->wakeup_tab) / sizeof(u32));
	if (ret) {
		dev_err(chip->dev, "please config keymap item %d\n", ret);
		kfree(chip->wakeup_tab);
		return -ENODATA;
	}

	meson_ir_mbox_set_wakeup_key(chip, key_num);
	return 0;
}

static int meson_ir_get_custom_tables(struct device_node *node,
				      struct meson_ir_chip *chip)
{
	const phandle *phandle;
	struct device_node *custom_maps, *map;
	u32 value;
	int ret = -1;
	int index;
	char *propname;
	const char *uname;
	unsigned long flags;
	struct meson_ir_map_tab_list *ptable;

	phandle = of_get_property(node, "map", NULL);
	if (!phandle) {
		dev_err(chip->dev, "%s:can't find match custom\n", __func__);
		return -EINVAL;
	}

	custom_maps = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!custom_maps) {
		dev_err(chip->dev, "can't find device node key\n");
		return -ENODATA;
	}

	ret = of_property_read_u32(custom_maps, "map_num", &value);
	if (ret) {
		dev_err(chip->dev, "please config correct map num item\n");
		return -ENODATA;
	}
	chip->custom_num = value;
	if (chip->custom_num > CUSTOM_NUM_MAX)
		chip->custom_num = CUSTOM_NUM_MAX;

	for (index = 0; index < chip->custom_num; index++) {
		propname = kasprintf(GFP_KERNEL, "map%d", index);
		phandle = of_get_property(custom_maps, propname, NULL);
		/* propname is never used just use to find phandle once
		 * To avoid kmemleak warning, should be freed
		 */
		kfree(propname);
		propname = NULL;

		if (!phandle) {
			dev_err(chip->dev, "can't find match map%d\n", index);
			return -ENODATA;
		}
		map = of_find_node_by_phandle(be32_to_cpup(phandle));
		if (!map) {
			dev_err(chip->dev, "can't find device node key\n");
			return -ENODATA;
		}

		ret = of_property_read_u32(map, "size", &value);
		if (ret || value > MAX_KEYMAP_SIZE) {
			dev_err(chip->dev, "no config size item or err\n");
			return -ENODATA;
		}

		/*alloc memory*/
		ptable = kzalloc(sizeof(*ptable) +
				 value * sizeof(union _codemap), GFP_KERNEL);
		if (!ptable)
			return -ENOMEM;

		ptable->tab.map_size = value;

		ret = of_property_read_string(map, "mapname", &uname);
		if (ret) {
			dev_err(chip->dev, "please config mapname item\n");
			goto err;
		}
		snprintf(ptable->tab.custom_name, CUSTOM_NAME_LEN, "%s", uname);

		ret = of_property_read_u32(map, "customcode", &value);
		if (ret) {
			dev_err(chip->dev, "please config customcode item\n");
			goto err;
		}
		ptable->tab.custom_code = value;

		ret = of_property_read_u32(map, "release_delay", &value);
		if (ret) {
			dev_err(chip->dev,
				"can't find the node <release_delay>\n");
			goto err;
		}
		ptable->tab.release_delay = value;

		ret = of_property_read_u32(map, "vendor", &value);
		if (ret) {
			ret = of_property_read_u32(node, "vendor", &value);
			if (ret)
				value = 0x0001;
		}
		ptable->tab.id.vendor = value;

		ret = of_property_read_u32(map, "product", &value);
		if (ret) {
			ret = of_property_read_u32(node, "product", &value);
			if (ret)
				value = 0x0001;
		}
		ptable->tab.id.product = value;

		ret = of_property_read_u32(map, "version", &value);
		if (ret)
			value = 0x0100;
		ptable->tab.id.version = value;

		ptable->tab.id.bustype = BUS_ISA;

		ret = of_property_read_u32_array(map, "keymap",
						 (u32 *)&ptable->tab.codemap[0],
						 ptable->tab.map_size);
		if (ret) {
			dev_err(chip->dev, "please config keymap item\n");
			goto err;
		}

		memset(&ptable->tab.cursor_code, 0xff,
		       sizeof(struct cursor_codemap));
		meson_ir_scancode_sort(&ptable->tab);
		/*insert list*/
		spin_lock_irqsave(&chip->slock, flags);
		list_add_tail(&ptable->list, &chip->map_tab_head);
		spin_unlock_irqrestore(&chip->slock, flags);
	}
	return 0;
err:
	meson_ir_tab_free(ptable);
	return -ENODATA;
}

static struct regmap_config meson_ir_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int meson_ir_get_devtree_pdata(struct platform_device *pdev)
{
	struct resource *res_mem;
	struct regmap *reg_base;
	resource_size_t *res_start[2];
	int ret;
	int value = 0;
	unsigned char i;

	struct meson_ir_chip *chip = platform_get_drvdata(pdev);

	chip->mbox_chan = aml_mbox_request_channel_byidx(&pdev->dev, 0);
	if (IS_ERR_OR_NULL(chip->mbox_chan))
		dev_err(chip->dev, "can't request mbox\n");

	ret = of_property_read_u32(pdev->dev.of_node,
				   "protocol", &chip->protocol);
	if (ret) {
		dev_err(chip->dev, "can't find the node <protocol>\n");
		chip->protocol = 1;
	}

	ret = of_property_read_bool(pdev->dev.of_node, "use_fifo");
	if (ret)
		chip->r_dev->use_fifo = 1;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "led_blink", &chip->r_dev->led_blink);
	if (ret)
		chip->r_dev->led_blink = 0;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "led_blink_frq", &value);
	if (ret) {
		chip->r_dev->delay_on = DEFAULT_LED_BLINK_FRQ;
		chip->r_dev->delay_off = DEFAULT_LED_BLINK_FRQ;
	} else {
		chip->r_dev->delay_off = value;
		chip->r_dev->delay_on = value;
	}

	for (i = 0; i < 2; i++) {
		res_mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (IS_ERR_OR_NULL(res_mem)) {
			dev_err(chip->dev, "get IORESOURCE_MEM error, %ld\n",
				PTR_ERR(res_mem));
			return PTR_ERR(res_mem);
		}
		res_start[i] = devm_ioremap_resource(&pdev->dev, res_mem);
		chip->ir_contr[i].remote_regs = (void __iomem *)res_start[i];
		meson_ir_regmap_config.max_register =
			resource_size(res_mem) - 4;
		meson_ir_regmap_config.name = devm_kasprintf(&pdev->dev,
							     GFP_KERNEL,
							     "%d", i);
		reg_base = devm_regmap_init_mmio(&pdev->dev, res_start[i],
						 &meson_ir_regmap_config);

		if (IS_ERR(reg_base)) {
			dev_err(chip->dev, "regmap init error, %ld\n",
				PTR_ERR(reg_base));

			return PTR_ERR(reg_base);
		}
		chip->ir_contr[i].base = reg_base;

		chip->irqno[i] = platform_get_irq_optional(pdev, i);
	}

	if (chip->irqno[0] < 0) {
		dev_err(chip->dev, "get IORESOURCE_IRQ error, %d\n",
			chip->irqno[0]);
		return chip->irqno[0];
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "max_frame_time", &value);
	if (ret) {
		dev_err(chip->dev, "can't find the node <max_frame_time>\n");
		value = 200; /*default value*/
	}

	chip->r_dev->max_frame_time = value;

	/*create map table */
	meson_ir_get_custom_tables(pdev->dev.of_node, chip);

	meson_ir_get_wakeup_tables(pdev->dev.of_node, chip);

	return 0;
}

static int meson_ir_hardware_init(struct platform_device *pdev)
{
	int ret;
	unsigned char cnt;

	struct meson_ir_chip *chip = platform_get_drvdata(pdev);

	if (!pdev->dev.of_node) {
		dev_err(chip->dev, "pdev->dev.of_node == NULL!\n");
		return -ENODATA;
	}

	ret = meson_ir_get_devtree_pdata(pdev);
	if (ret < 0)
		return ret;

	chip->set_register_config(chip, chip->protocol);
	chip->irq_cpumask = 1;

	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2 : 1); cnt++) {
		if (chip->irqno[cnt] < 0)
			continue;

		ret = devm_request_irq(&pdev->dev, chip->irqno[cnt],
				       meson_ir_interrupt,
				       IRQF_SHARED | IRQF_NO_SUSPEND, chip->dev_name,
				       (void *)chip);
		if (ret < 0) {
			dev_err(chip->dev, "request_irq error %d\n", ret);
			return -ENODEV;
		}

		irq_set_affinity_hint(chip->irqno[cnt],
				      cpumask_of(chip->irq_cpumask));
	}
	meson_ir_set_irq(chip, 0);

	return 0;
}

static int meson_ir_input_device_init(struct device *parent)
{
	int ret;
	struct meson_ir_chip *chip = dev_get_drvdata(parent);
	struct meson_ir_dev *r_dev = chip->r_dev;
	struct meson_ir_map_tab_list *ir_map = NULL;
	struct input_dev *input_device;
	unsigned int match_cnt = 0, i;
	struct input_id *match_id;
	const char *name;

	match_id = kcalloc(chip->custom_num, sizeof(*match_id), GFP_KERNEL);
	if (!match_id)
		return -ENOMEM;

	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		for (i = 0; i < match_cnt; i++)
			if (!memcmp(&ir_map->tab.id, &match_id[i],
				    sizeof(*match_id)))
				break;

		if (i == match_cnt) {
			memcpy(&match_id[match_cnt], &ir_map->tab.id,
			       sizeof(*match_id));
			match_cnt++;
		}
	}

	kfree(match_id);

	if (match_cnt > 0)
		r_dev->input_devs = kcalloc(match_cnt, sizeof(struct input_dev *),
					    GFP_KERNEL);
	else
		r_dev->input_devs = NULL;

	r_dev->input_dev_num = 0;

	if (r_dev->input_devs == NULL)
		return 0;

	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		input_device = meson_ir_match_input_dev(r_dev, ir_map);
		if (input_device) {
			meson_ir_input_configure(input_device, &ir_map->tab);
			continue;
		}

		input_device = devm_input_allocate_device(chip->dev);
		if (!input_device)
			return -ENOMEM;

		name = devm_kasprintf(parent, GFP_KERNEL, "ir_keypad%d_%d",
				      chip->dev_no, r_dev->input_dev_num);

		input_device->name = name;
		input_device->phys = "keypad/input0";
		input_device->dev.parent = parent;
		memcpy(&input_device->id, &ir_map->tab.id,
		       sizeof(struct input_id));
		input_device->rep[REP_DELAY] = 0xffffffff;  /*close input repeat*/
		input_device->rep[REP_PERIOD] = 0xffffffff; /*close input repeat*/

		meson_ir_input_configure(input_device, &ir_map->tab);

		input_set_drvdata(input_device, r_dev);
		ret = input_register_device(input_device);
		if (ret < 0)
			return ret;

		r_dev->input_devs[r_dev->input_dev_num] = input_device;
		r_dev->input_dev_num++;
	}
	return 0;
}

static int meson_ir_probe(struct platform_device *pdev)
{
	struct meson_ir_dev *dev;
	struct meson_ir_chip *chip;
	int ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct meson_ir_chip),
			    GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->keylock);
	dev->wait_next_repeat = 0;
	dev->is_probed = 1;
	dev->enable = !disable_ir;

	chip->dev_no = atomic_inc_return(&meson_ir_dev_no);
	chip->dev_name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
					"amremote");

	INIT_WORK(&chip->ir_workqueue, meson_ir_workqueue);
	INIT_WORK(&chip->get_fifo_workqueue, meson_ir_get_fifo_work);
	tasklet_setup(&chip->tasklet, meson_ir_tasklet);
	chip->tasklet.data = (unsigned long)chip;

	mutex_init(&chip->file_lock);
	spin_lock_init(&chip->slock);
	INIT_LIST_HEAD(&chip->map_tab_head);

	chip->r_dev = dev;
	chip->dev = &pdev->dev;
	chip->rx_count = 0;

	chip->r_dev->dev = &pdev->dev;
	chip->r_dev->platform_data = (void *)chip;
	chip->r_dev->getkeycode = meson_ir_getkeycode;
	chip->r_dev->report_rel = meson_ir_report_rel;
	chip->r_dev->set_custom_code = meson_ir_set_custom_code;
	chip->r_dev->is_valid_custom = meson_ir_is_valid_custom;
	chip->r_dev->is_next_repeat  = meson_ir_is_next_repeat;
	chip->r_dev->max_learned_pulse = MAX_LEARNED_PULSE;
	chip->set_register_config = meson_ir_register_default_config;
	platform_set_drvdata(pdev, chip);

	ret = meson_ir_hardware_init(pdev);
	if (ret < 0)
		return ret;

	ret = meson_ir_input_device_init(&pdev->dev);
	if (ret < 0)
		return ret;

	timer_setup(&dev->timer_keyup, meson_ir_timer_keyup, 0);

	ret = meson_ir_cdev_init(chip);
	if (ret < 0)
		return ret;

	dev->rc_type = chip->protocol;

	device_init_wakeup(&pdev->dev, true);

	led_trigger_register_simple("ir_led", &dev->led_feedback);

	if (MULTI_IR_SOFTWARE_DECODE(dev->rc_type))
		meson_ir_raw_event_register(dev);

	dev_pm_set_wake_irq(&pdev->dev, chip->irqno[0]);
	if (chip->irqno[1] >= 0)
		dev_pm_set_wake_irq(&pdev->dev, chip->irqno[1]);

	if (dev->enable)
		meson_ir_set_irq(chip, 1);

	return 0;
}

static void meson_ir_remove(struct platform_device *pdev)
{
	int i;
	struct meson_ir_chip *chip = platform_get_drvdata(pdev);

	meson_ir_set_irq(chip, 0);
	del_timer(&chip->r_dev->timer_keyup);
	tasklet_kill(&chip->tasklet);

	if (MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type))
		meson_ir_raw_event_unregister(chip->r_dev);

	led_trigger_unregister_simple(chip->r_dev->led_feedback);

	dev_pm_clear_wake_irq(&pdev->dev);
	device_init_wakeup(&pdev->dev, false);

	meson_ir_cdev_free(chip);

	for (i = 0; i < chip->r_dev->input_dev_num; i++)
		input_unregister_device(chip->r_dev->input_devs[i]);
	kfree(chip->r_dev->input_devs);
	kfree(chip->wakeup_tab);

	meson_ir_map_tab_list_free(chip);
	irq_set_affinity_hint(chip->irqno[0], NULL);
	if (chip->irqno[1] >= 0)
		irq_set_affinity_hint(chip->irqno[1], NULL);

	atomic_dec_return(&meson_ir_dev_no);
}

static void meson_ir_shutdown(struct platform_device *pdev)
{
	struct meson_ir_chip *chip = platform_get_drvdata(pdev);
	u32 data[3] = {IR_MBOX_CMD_SET_STATUS, 0};

	meson_ir_mbox_transfer(chip, data, sizeof(data));

	meson_ir_remove(pdev);
}

static int meson_ir_resume(struct device *dev)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	unsigned int val;
	unsigned long flags;
	unsigned char cnt;
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unsigned int data = IR_MBOX_CMD_GET_WAKEUP_KEY;
#endif

	chip->r_dev->is_probed = 0;
	/*resume register config*/
	spin_lock_irqsave(&chip->slock, flags);
	chip->set_register_config(chip, chip->protocol);
	/* read REG_STATUS and REG_FRAME to clear status */
	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2 : 1); cnt++) {
		regmap_read(chip->ir_contr[cnt].base, REG_STATUS, &val);
		regmap_read(chip->ir_contr[cnt].base, REG_FRAME, &val);
	}
	spin_unlock_irqrestore(&chip->slock, flags);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
		meson_ir_mbox_transfer(chip, &data, sizeof(data));
		meson_ir_report_wakeup_event(chip, data);
#endif

	irq_set_affinity_hint(chip->irqno[0], cpumask_of(chip->irq_cpumask));
	if (chip->irqno[1] >= 0)
		irq_set_affinity_hint(chip->irqno[1],
				      cpumask_of(chip->irq_cpumask));
	if (is_pm_s2idle_mode())
		return 0;
	meson_ir_set_irq(chip, 1);
	return 0;
}

static int meson_ir_suspend(struct device *dev)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);

	if (is_pm_s2idle_mode())
		return 0;
	meson_ir_set_irq(chip, 0);

	return 0;
}

static int meson_ir_freeze(struct device *dev)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);

	meson_ir_set_irq(chip, 0);

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int meson_ir_restore(struct device *dev)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	unsigned int val;
	unsigned long flags;
	unsigned char cnt;
	unsigned int key_num = 0;

	pinctrl_pm_select_default_state(dev);

	/*resume register config*/
	spin_lock_irqsave(&chip->slock, flags);
	chip->set_register_config(chip, chip->protocol);
	/* read REG_STATUS and REG_FRAME to clear status */
	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2 : 1); cnt++) {
		regmap_read(chip->ir_contr[cnt].base, REG_STATUS, &val);
		regmap_read(chip->ir_contr[cnt].base, REG_FRAME, &val);
	}
	spin_unlock_irqrestore(&chip->slock, flags);

	while (chip->wakeup_tab[key_num].frame_code)
		key_num++;
	meson_ir_mbox_set_wakeup_key(chip, key_num);

	meson_ir_set_irq(chip, 1);

	return 0;
}

static const struct of_device_id meson_ir_dt_match[] = {
	{
		.compatible     = "amlogic, meson-ir",
	},
	{},
};

#ifdef CONFIG_PM
static const struct dev_pm_ops meson_ir_pm_ops = {
	.suspend_late = meson_ir_suspend,
	.resume_early = meson_ir_resume,
	.freeze = meson_ir_freeze,
	.restore = meson_ir_restore,
};
#endif

static struct platform_driver meson_ir_driver = {
	.probe = meson_ir_probe,
	.remove = meson_ir_remove,
	.shutdown = meson_ir_shutdown,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_ir_dt_match,
#ifdef CONFIG_PM
		.pm = &meson_ir_pm_ops,
#endif
	},
};

int __init meson_ir_driver_init(void)
{
	int ret;

	ret = meson_ir_xmp_decode_init();
	if (ret)
		return ret;

	ret = meson_ir_common_input_init();
	if (ret)
		return ret;

	return platform_driver_register(&meson_ir_driver);
}

void __exit meson_ir_driver_exit(void)
{
	meson_ir_common_input_exit();
	meson_ir_xmp_decode_exit();
	platform_driver_unregister(&meson_ir_driver);
}
