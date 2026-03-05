// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/amlogic/media/resource_mgr/param_set.h>

#define INIT_CAPACITY 16

/* String tables for trace printing */
static const char *const domain_strings[DOMAIN_COUNT] = {
	[DOMAIN_NONE]  = "none",
	[DOMAIN_VIDEO]  = "video",
	[DOMAIN_DEVICE] = "device",
	[DOMAIN_SYSTEM] = "system",
	/* keep in sync with enum */
};

static const char *const sub_domain_strings[SUB_DOMAIN_COUNT] = {
	[SUB_DOMAIN_NONE]     = "none",
	[SUB_DOMAIN_DECODING] = "decoding",
	[SUB_DOMAIN_CHIP]     = "chip",
	[SUB_DOMAIN_MEMORY]   = "memory",
	[SUB_DOMAIN_PROFILE]  = "profile",
	/* keep in sync with enum */
};

const char *domain_to_string(__u32 d)
{
	if ((unsigned int)d < (unsigned int)DOMAIN_COUNT && domain_strings[d])
		return domain_strings[d];
	return NULL;
}

const char *sub_domain_to_string(__u32 sd)
{
	if ((unsigned int)sd < (unsigned int)SUB_DOMAIN_COUNT && sub_domain_strings[sd])
		return sub_domain_strings[sd];
	return NULL;
}

static const char *resman_param_key_names[RESMAN_PARAM_VIDEO_MAX] = {
	[RESMAN_PARAM_VIDEO_W]        = "video_w",
	[RESMAN_PARAM_VIDEO_H]        = "video_h",
	[RESMAN_PARAM_VIDEO_DW]       = "video_dw",
	[RESMAN_PARAM_VIDEO_FMT]       = "video_f",
	[RESMAN_PARAM_VIDEO_MARGIN]   = "video_m",
	[RESMAN_PARAM_VIDEO_FD]       = "video_fd",
	[RESMAN_PARAM_VIDEO_DPB]      = "video_dpb",
	[RESMAN_PARAM_VIDEO_SECURE]   = "video_s",
	[RESMAN_PARAM_VIDEO_PIPELINE] = "video_p",
	[RESMAN_PARAM_VIDEO_TOTALNUM] = "video_t",
	[RESMAN_PARAM_VIDEO_BITDEPTH] = "video_bitd",
	[RESMAN_PARAM_VIDEO_STATUS] = "video_std"
};

const char *get_resman_param_key_string(__u32 key)
{
	if (key >= RESMAN_PARAM_VIDEO_MAX)
		return NULL;
	return resman_param_key_names[key];
}

static int param_set_expand_if_needed(struct resman_param_set *set);

static void param_item_destroy(struct resman_param_item *item)
{
	if (!item)
		return;

	if (item->type == PARAM_TYPE_STRING && item->value.s) {
		kfree(item->value.s);
		item->value.s = NULL;
	}
	if (item->type == PARAM_TYPE_CUSTOM) {
		if (item->value.custom_set) {
			param_set_destroy(item->value.custom_set); /* nested set */
			item->value.custom_set = NULL;
		}
		if (item->value.custom_profile) {
			kfree(item->value.custom_profile->profile_name);
			kfree(item->value.custom_profile);
			item->value.custom_profile = NULL;
		}
	}
}

struct resman_param_set *param_set_create(void)
{
	struct resman_param_set *set = kzalloc(sizeof(*set), GFP_KERNEL);

	if (!set)
		return NULL;

	set->capacity = INIT_CAPACITY;
	set->size = 0;
	set->items = kcalloc(INIT_CAPACITY, sizeof(struct resman_param_item), GFP_KERNEL);
	if (!set->items) {
		kfree(set);
		return NULL;
	}
	return set;
}

void param_set_destroy(struct resman_param_set *set)
{
	size_t i;

	if (!set)
		return;
	for (i = 0; i < set->size; ++i)
		param_item_destroy(&set->items[i]);
	kfree(set->items);
	kfree(set);
}

/* Expand capacity using krealloc; zero-initialize the newly added tail */
static int param_set_expand_if_needed(struct resman_param_set *set)
{
	struct resman_param_item *tmp;
	size_t new_cap;

	if (!set)
		return 0;
	if (set->size < set->capacity)
		return 1;

	new_cap = set->capacity ? (set->capacity << 1) : INIT_CAPACITY;
	tmp = krealloc(set->items, new_cap * sizeof(struct resman_param_item), GFP_KERNEL);
	if (!tmp)
		return 0;

	/* zero the extended region */
	if (new_cap > set->capacity)
		memset(tmp + set->capacity, 0,
			(new_cap - set->capacity) * sizeof(struct resman_param_item));

	set->items = tmp;
	set->capacity = new_cap;
	return 1;
}

/* Meta fill helper: enums + IDs + module_name */
static inline int fill_meta(struct resman_param_item *item,
				const char *module_name,
				__u32 dom, __u32 sub,
				int inst, int pid, int tid)
{
	size_t len;

	if (!item)
		return 0;
	if (module_name) {
		len = strnlen(module_name, RESMAN_ITEM_LEN);
		if (len == 0) {
			pr_err("resman: invalid module_name\n");
			return 0;
		}
		len = (len >= RESMAN_ITEM_LEN) ? (RESMAN_ITEM_LEN - 1) : len;
		memcpy(item->module_name, module_name, len);
		item->module_name[len] = '\0';
	} else {
		item->module_name[0] = '\0';
	}
	item->domain      = dom;
	item->sub_domain  = sub;
	item->instance_id = inst;
	item->process_id  = pid;
	item->thread_id   = tid;
	return 1;
}

static int update_meta(struct resman_param_item *item,
			const char *module_name,
			__u32 dom, __u32 sub,
			int inst, int pid, int tid)
{
	size_t len;

	/* module_name: update*/
	if (!item)
		return 0;
	if (module_name) {
		len = strnlen(module_name, RESMAN_ITEM_LEN);
		if (len == 0) {
			pr_err("resman: invalid module_name\n");
			return 0;
		}
		len = (len >= RESMAN_ITEM_LEN) ? (RESMAN_ITEM_LEN - 1) : len;
		memcpy(item->module_name, module_name, len);
		item->module_name[len] = '\0';
	} else {
		item->module_name[0] = '\0';
	}
	item->domain      = dom;
	item->sub_domain  = sub;
	item->instance_id = inst;
	item->process_id  = pid;
	item->thread_id   = tid;
	return 1;
}

static int update_desc(struct resman_param_item *item, const char *desc)
{
	size_t len;

	if (!item || !desc)
		return 0;
	len = strnlen(desc, RESMAN_ITEM_LEN);
	if (len >= RESMAN_ITEM_LEN)
		return 0;
	memcpy(item->desc, desc, len);
	item->desc[len] = '\0';
	return 1;
}

static int check_type_or_complain(struct resman_param_item *item,
			__u32 expect, const char *name)
{
	if (item->type != expect) {
		pr_err("resman: type mismatch on update for \"%s\" (have=%d, expect=%d)\n",
		       name, item->type, expect);
		return 0;
	}
	return 1;
}

/* Add functions */
static int param_set_add_int(struct resman_param_set *set, const char *name,
				int value, const char *desc,
				const char *module_name, __u32 domain,
				__u32 sub_domain, int instance_id,
				int process_id, int thread_id)
{
	struct resman_param_item  *item;
	size_t len;

	if (!set || !name)
		return 0;
	if (!param_set_expand_if_needed(set))
		return 0;
	if (set->size >= set->capacity)
		return 0;

	item = &set->items[set->size];
	memset(item, 0, sizeof(*item));
	len = strnlen(name, RESMAN_ITEM_LEN);
	memcpy(item->name, name, len);
	item->name[sizeof(item->name) - 1] = '\0';

	item->type = PARAM_TYPE_INT;
	if (!fill_meta(item, module_name, domain, sub_domain, instance_id, process_id, thread_id))
		return 0;
	item->value.i = value;
	len = strnlen(desc, RESMAN_ITEM_LEN);
	memcpy(item->desc, desc, len);
	set->size++;
	return 1;
}

static int param_set_add_int64(struct resman_param_set *set, const char *name, s64 value,
			const char *desc, const char *module_name,
			__u32 domain, __u32 sub_domain,
			int instance_id, int process_id, int thread_id)
{
	struct resman_param_item *item;
	size_t len;

	if (!set || !name)
		return 0;
	if (!param_set_expand_if_needed(set))
		return 0;
	if (set->size >= set->capacity)
		return 0;

	item = &set->items[set->size++];
	memset(item, 0, sizeof(*item));
	len = strnlen(name, RESMAN_ITEM_LEN);
	memcpy(item->name, name, len);
	item->name[sizeof(item->name) - 1] = '\0';

	item->type = PARAM_TYPE_INT64;
	if (!fill_meta(item, module_name, domain, sub_domain,
		       instance_id, process_id, thread_id))
		return 0;

	item->value.i64 = value;
	len = strnlen(desc, RESMAN_ITEM_LEN);
	memcpy(item->desc, desc, len);

	return 1;
}

static int param_set_add_string(struct resman_param_set *set, const char *name, char *value,
			const char *desc, const char *module_name, __u32 domain,
			__u32 sub_domain, int instance_id,
			int process_id, int thread_id)
{
	struct resman_param_item *item;
	size_t len;

	if (!set || !name || !value)
		return 0;
	if (!param_set_expand_if_needed(set))
		return 0;
	if (set->size >= set->capacity)
		return 0;

	item = &set->items[set->size++];
	memset(item, 0, sizeof(*item));
	len = strnlen(name, RESMAN_ITEM_LEN);
	memcpy(item->name, name, len);
	item->name[sizeof(item->name) - 1] = '\0';

	item->type = PARAM_TYPE_STRING;
	if (!fill_meta(item, module_name, domain, sub_domain,
		instance_id, process_id, thread_id))
		return 0;
	item->value.s = kstrdup(value, GFP_KERNEL);
	if (!item->value.s)
		return 0;
	len = strnlen(desc, RESMAN_ITEM_LEN);
	memcpy(item->desc, desc, len);
	return 1;
}

static int param_set_add_custom_set(struct resman_param_set *set, const char *name,
					struct resman_param_set *custom, const char *desc,
					const char *module_name, __u32 domain,
					__u32 sub_domain, int instance_id,
					int process_id, int thread_id)
{
	struct resman_param_item *item;
	size_t len;

	if (!set || !name)
		return 0;
	if (!param_set_expand_if_needed(set))
		return 0;
	if (set->size >= set->capacity)
		return 0;

	item = &set->items[set->size++];
	memset(item, 0, sizeof(*item));
	len = strnlen(name, RESMAN_ITEM_LEN);
	memcpy(item->name, name, len);

	item->type = PARAM_TYPE_CUSTOM;
	if (!fill_meta(item, module_name, domain, sub_domain,
				instance_id, process_id, thread_id))
		return 0;
	item->value.custom_set = custom;
	/* take ownership; destroyed in param_item_destroy */
	item->value.custom_profile = NULL;
	len = strnlen(desc, RESMAN_ITEM_LEN);
	memcpy(item->desc, desc, len);
	return 1;
}

static int param_set_add_custom_profile(struct resman_param_set *set, const char *name,
				struct resman_custom_media_profile *profile, const char *desc,
				const char *module_name, __u32 domain, __u32 sub_domain,
				int instance_id, int process_id, int thread_id)
{
	struct resman_param_item *item;
	size_t len;

	if (!set || !name || !profile)
		return 0;
	if (!param_set_expand_if_needed(set))
		return 0;
	if (set->size >= set->capacity)
		return 0;

	item = &set->items[set->size++];
	memset(item, 0, sizeof(*item));
	len = strnlen(name, RESMAN_ITEM_LEN);
	memcpy(item->name, name, len);

	item->type = PARAM_TYPE_CUSTOM;
	if (!fill_meta(item, module_name, domain, sub_domain,
			instance_id, process_id, thread_id))
		return 0;
	item->value.custom_set = NULL;
	item->value.custom_profile = profile;
	len = strnlen(desc, RESMAN_ITEM_LEN);
	memcpy(item->desc, desc, len);
	return 1;
}

/* Retrieve parameter information by name or index */
struct resman_param_item *param_set_get_by_name(struct resman_param_set *set, const char *name)
{
	size_t i;
	size_t name_len;

	if (!set || !name)
		return NULL;
	name_len = strnlen(name, RESMAN_ITEM_LEN);
	if (name_len == RESMAN_ITEM_LEN)
		return NULL;
	for (i = 0; i < set->size; ++i) {
		size_t item_name_len;

		item_name_len = strnlen(set->items[i].name, RESMAN_ITEM_LEN);
		if (item_name_len == RESMAN_ITEM_LEN)
			continue;
		if (item_name_len == name_len &&
			!strncmp(set->items[i].name, name, name_len))
			return &set->items[i];
	}
	return NULL;
}

struct resman_param_item *param_set_get_by_index(struct resman_param_set *set, size_t idx)
{
	if (!set)
		return NULL;
	if (idx < set->size)
		return &set->items[idx];
	return NULL;
}

/* Two-level query by domain and sub_domain
 * Caller must kfree() the returned array (struct resman_param_item**), not the elements.
 */
struct resman_param_item **param_set_get_by_domain(struct resman_param_set *set,
						__u32 domain, __u32 sub_domain, size_t *out_count)
{
	size_t i, count = 0, j = 0;
	struct resman_param_item **result;

	if (out_count)
		*out_count = 0;
	if (!set)
		return NULL;

	/* First pass: count matches */
	for (i = 0; i < set->size; ++i) {
		struct resman_param_item *it = &set->items[i];

		if (it->domain != domain)
			continue;
		if (sub_domain != SUB_DOMAIN_NONE) {
			if (it->sub_domain != sub_domain)
				continue;
		}
		++count;
	}

	if (count == 0)
		return NULL;

	/* Second pass: collect pointers */
	result = kmalloc_array(count, sizeof(struct resman_param_item *), GFP_KERNEL);
	if (!result)
		return NULL;

	for (i = 0; i < set->size; ++i) {
		struct resman_param_item *it = &set->items[i];

		if (it->domain != domain)
			continue;
		if (sub_domain != SUB_DOMAIN_NONE) {
			if (it->sub_domain != sub_domain)
				continue;
		}
		result[j++] = it;
	}

	if (out_count)
		*out_count = count;

	return result;
}

int param_set_update_int(struct resman_param_set *set, const char *name,
			int value, const char *desc,
			const char *module_name, __u32 domain,
			__u32 sub_domain, __u32 instance_id,
			__u32 process_id, __u32 thread_id)
{
	struct resman_param_item *item;

	if (!set || !name)
		return 0;

	item = param_set_get_by_name(set, name);
	if (!item) {
		return param_set_add_int(set, name, value, desc, module_name,
					 domain, sub_domain, instance_id, process_id, thread_id);
	}
	if (!check_type_or_complain(item, PARAM_TYPE_INT, name))
		return 0;

	item->value.i = value;

	if (!update_desc(item, desc))
		return 0;

	if (!update_meta(item, module_name, domain, sub_domain, instance_id, process_id, thread_id))
		return 0;

	return 1;
}

int param_set_update_int64(struct resman_param_set *set, const char *name, s64 value,
			const char *desc, const char *module_name,
			__u32 domain, __u32 sub_domain,
			__u32 instance_id, __u32 process_id, __u32 thread_id)
{
	struct resman_param_item *item;

	if (!set || !name)
		return 0;

	item = param_set_get_by_name(set, name);
	if (!item) {
		return param_set_add_int64(set, name, value, desc, module_name, domain,
					   sub_domain, instance_id, process_id, thread_id);
	}

	if (!check_type_or_complain(item, PARAM_TYPE_INT64, name))
		return 0;

	item->value.i64 = value;

	if (!update_desc(item, desc))
		return 0;
	if (!update_meta(item, module_name, domain, sub_domain, instance_id, process_id, thread_id))
		return 0;

	return 1;
}

int param_set_update_string(struct resman_param_set *set, const char *name, char *value,
			const char *desc, const char *module_name,
			__u32 domain, __u32 sub_domain,
			__u32 instance_id, __u32 process_id, __u32 thread_id)
{
	struct resman_param_item *item;
	char *dup = NULL;

	if (!set || !name || !value)
		return 0;

	item = param_set_get_by_name(set, name);
	if (!item) {
		return param_set_add_string(set, name, value, desc, module_name, domain,
					    sub_domain, instance_id, process_id, thread_id);
	}

	if (!check_type_or_complain(item, PARAM_TYPE_STRING, name))
		return 0;

	dup = kstrdup(value, GFP_KERNEL);
	if (!dup)
		return 0;

	kfree(item->value.s);
	item->value.s = NULL;
	item->value.s = dup;

	if (!update_desc(item, desc))
		return 0;
	if (!update_meta(item, module_name, domain, sub_domain, instance_id, process_id, thread_id))
		return 0;

	return 1;
}

int param_set_update_custom_set(struct resman_param_set *set, const char *name,
				struct resman_param_set *custom, const char *desc,
				const char *module_name, __u32 domain,
				__u32 sub_domain, __u32 instance_id,
				__u32 process_id, __u32 thread_id)
{
	struct resman_param_item *item;

	if (!set || !name)
		return 0;

	item = param_set_get_by_name(set, name);
	if (!item) {
		return param_set_add_custom_set(set, name, custom, desc, module_name,
					domain, sub_domain, instance_id, process_id, thread_id);
	}

	if (!check_type_or_complain(item, PARAM_TYPE_CUSTOM, name))
		return 0;
	if (item->value.custom_set) {
		param_set_destroy(item->value.custom_set);
		item->value.custom_set = NULL;
	}
	if (item->value.custom_profile) {
		kfree(item->value.custom_profile->profile_name);
		kfree(item->value.custom_profile);
		item->value.custom_profile = NULL;
	}

	item->value.custom_set = custom;
	item->value.custom_profile = NULL;

	if (!update_desc(item, desc))
		return 0;
	if (!update_meta(item, module_name, domain, sub_domain, instance_id, process_id, thread_id))
		return 0;

	return 1;
}

int param_set_update_custom_profile(struct resman_param_set *set, const char *name,
				struct resman_custom_media_profile *profile, const char *desc,
				const char *module_name, __u32 domain,
				__u32 sub_domain, __u32 instance_id,
				__u32 process_id, __u32 thread_id)
{
	struct resman_param_item *item;

	if (!set || !name || !profile)
		return 0;

	item = param_set_get_by_name(set, name);
	if (!item) {
		return param_set_add_custom_profile(set, name, profile, desc, module_name,
						    domain, sub_domain, instance_id,
						    process_id, thread_id);
	}

	if (!check_type_or_complain(item, PARAM_TYPE_CUSTOM, name))
		return 0;

	if (item->value.custom_set) {
		param_set_destroy(item->value.custom_set);
		item->value.custom_set = NULL;
	}
	if (item->value.custom_profile) {
		kfree(item->value.custom_profile->profile_name);
		kfree(item->value.custom_profile);
		item->value.custom_profile = NULL;
	}

	item->value.custom_set = NULL;
	item->value.custom_profile = profile;

	if (!update_desc(item, desc))
		return 0;
	if (!update_meta(item, module_name, domain, sub_domain, instance_id, process_id, thread_id))
		return 0;

	return 1;
}

/* Custom profile trace: avoid FP formatting that implies FP ops;
 * we only print the bit pattern via %pK for pointer and use %d for ints.
 * For float, to avoid enabling FPU, we print as integer bit pattern using memcpy.
 */
void trace_custom_profile(struct resman_custom_media_profile *p, int indent)
{
	int i;

	if (!p)
		return;

	for (i = 0; i < indent; ++i)
		pr_info("  ");

	pr_info("{ profile_id=%d, profile_name=%s}\n",
				p->media_profile_id,
				p->profile_name ? p->profile_name : "NULL");
}

/* Debug trace: similarly avoid real FP prints; show float/double as raw bits */
void param_set_trace(struct resman_param_set *set, int indent)
{
	size_t i;
	int j;

	if (!set)
		return;

	for (i = 0; i < set->size; ++i) {
		struct resman_param_item *item = &set->items[i];

		for (j = 0; j < indent; ++j)
			pr_info("  ");
		pr_info("%s (type=%d): %s\n",
			item->name,
			(int)item->type,
			item->desc);

		for (j = 0; j < indent; ++j)
			pr_info("  ");

		{
			const char *dstr = domain_to_string(item->domain);
			const char *sstr = sub_domain_to_string(item->sub_domain);

			if (!dstr)
				dstr = "unknown";
			if (!sstr)
				sstr = "unknown";
			pr_info(" [module:%s domain:%s sub:%s inst:%d pid:%d tid:%d] ",
				item->module_name,
				dstr, sstr,
				item->instance_id, item->process_id, item->thread_id);
		}

		switch (item->type) {
		case PARAM_TYPE_INT:
			pr_info("int: %d\n", item->value.i);
			break;
		case PARAM_TYPE_INT64:
			pr_info("int: %lld\n", item->value.i64);
			break;
		case PARAM_TYPE_STRING:
			pr_info("str: %s\n", item->value.s ? item->value.s : "NULL");
			break;
		case PARAM_TYPE_CUSTOM:
			if (item->value.custom_set) {
				pr_info("custom set:\n");
				param_set_trace(item->value.custom_set, indent + 1);
			} else if (item->value.custom_profile) {
				pr_info("custom profile: ");
				trace_custom_profile(item->value.custom_profile, indent + 1);
			} else {
				pr_info("custom: NULL\n");
			}
			break;
		default:
			pr_info("unknown type\n");
			break;
		}
	}
}
