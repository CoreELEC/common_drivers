// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/compat.h>
#include <linux/kstrtox.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/resource_mgr/resourcemanage.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_processor/di_proc_buf_mgr.h>


#define RESMAM_ENABLE_JSON  (1)

#ifdef RESMAM_ENABLE_JSON
#include "cjson.h"
#endif

#define DEVICE_NAME "amresource_mgr"
#define DEVICE_CLASS_NAME "resource_mgr"

#define QUERY_SECURE_BUFFER (1)
#define QUERY_NO_SECURE_BUFFER (0)
#define RESMAN_VERSION         (4)// 4 support more accurate resource management
#define RESMAN_SUB_VERSION     (0)
#define RESMAN_SUBMIT_NUMBER   (0)
#define RESMAN_SINGLE_SIZE      (64)
#define RESMAN_STR_MAX_SIZE     (32)
#define RESMAN_DEMUX_VIDEO_SIZE (15)
#define RESMAN_DEMUX_AUDIO_SIZE (3)
#define RESMAN_DEMUX_CACHE_SIZE (6)
#define RESMAN_MAX_PRINT_LEN  500
#define RESMAN_DEFAULIT_MIN_SIZE_FOR_VDEC   (10)  //min codec mm size for base workspace buffer
#define RESMAN_DEFAULIT_EXPECT_SIZE_FOR_PLAYBACK   (60)  //expect codec mmsize for plabyack
#define RESMAN_BYTES_MAX   (1024 * 1024 * 1024)

struct {
	int id;
	char *name;
} resources_map[] = {
	{RESMAN_ID_VFM_DEFAULT, "vfm_default"},
	{RESMAN_ID_AMVIDEO, "amvideo"},
	{RESMAN_ID_PIPVIDEO, "videopip"},
	{RESMAN_ID_SEC_TVP, "tvp"},
	{RESMAN_ID_TSPARSER, "tsparser"},
	{RESMAN_ID_CODEC_MM, "codec_mm"},
	{RESMAN_ID_ADC_PLL, "adc_pll"},
	{RESMAN_ID_DECODER, "decoder"},
	{RESMAN_ID_HWC, "hwc"},
	{RESMAN_ID_DMX, "dmx"},
	{RESMAN_ID_DI, "di"},
	{RESMAN_ID_VDPLANE, "videoplane"},
	{RESMAN_ID_AUDIO_DECODER, "audiodecoder"},
};

enum RESMAN_STATUS {
	RESMAN_STATUS_INITED,
	RESMAN_STATUS_ACQUIRED,
	RESMAN_STATUS_PREEMPTED
};

/**
 * struct resman_event - event list node
 * @list: list entry
 * @type: event type, see RESMAN_EVENT
 */
struct resman_event {
	struct list_head list;
	__u32 type;
};

struct counter_res_state {
	int index;
	int ssid;
	int state;
};

struct counter_res_state_list {
	struct counter_res_state *items;
	int size;
};

/**
 * struct resman_node - allocated resource node
 * @slist: session list that own same resource
 * @rlist: resource list that own by same session
 * @pending_release: pending session for release
 * @session_ptr: session owner
 * @resource_ptr: resource owner
 * @s: node data
 */
struct resman_node {
	struct list_head slist;
	struct list_head rlist;
	struct completion pending_release;
	void *session_ptr;
	void *resource_ptr;
	union {
		struct {
			char type;
		} toggle;
		struct {
			__u32 score;
			bool secure;
		} codec_mm;
		struct {
			__u32 use;
		} capacity;
	} s;
};

/**
 * struct resman_session - session data
 * @id: session id
 * @list: sessions list
 * @resources: allocated resource list
 * @events: events list
 * @app_name: name of app
 * @status: session status, see RESMAN_STATUS
 * @comm: proc comm name
 * @pid: thread id
 * @tgid: pid
 * @lock: lock of session
 * @wq_event: wait queue for poll
 * @app_type: type of session, see RESMAN_APP
 */
struct resman_session {
	int id;
	struct list_head list;
	struct list_head resources;
	struct list_head events;
	char app_name[32];
	int status;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	pid_t tgid;
	struct mutex lock; /*session lock*/
	wait_queue_head_t wq_event;
	int app_type;
	int prio;
#ifdef RESMAN_ENABLE_PARAMETER_SET
	struct resman_param_set  *param_set;
#endif
};

/**
 * struct resman_resource - resource data
 * @list: resource list
 * @sessions: list of session that acquired this resource
 *            the first one always the oldest one
 * @lock: resource lock
 * @wq_release: wait queue for release
 * @wq_acquire: wait queue for acquire
 * @pending_acquire: pending session for acquire
 * @id: resource id
 * @name: name of resource
 * @type: resource tye, see RESMAN_TYPE
 * @acquire: hook of acquire
 * @release: hook of release
 * @value: value of resource
 * @d: resource data
 *
 */
struct resman_resource {
	struct list_head list;
	struct list_head sessions;
	struct mutex lock; /*resource lock*/
	wait_queue_head_t wq_release;
	wait_queue_head_t wq_acquire;
	atomic_t pending_acquire;
	int id;
	char name[32];
	__u32 type;
	bool (*acquire)(struct resman_session *sess,
			struct resman_resource *resource,
			struct resman_node *node,
			bool preempt,
			__u32 timeout,
			char *arg);
	void (*release)(struct resman_session *sess,
			struct resman_resource *resource,
			struct resman_node *node);
	int value;
	union {
		/**
		 * @avail: max available value for counter
		 */
		struct {
			__u32 avail;
			struct counter_res_state_list *counter_list;
		} counter;
		/**
		 * @total: total score
		 * @uhd: how much score per uhd instance
		 * @fhd: how much score per fhd instance
		 * @secure: max secure decoder count
		 */
		struct {
			__u32 total;
			__u32 uhd;
			__u32 fhd;
			__u32 secure;
			atomic_t counter_secure;
			atomic_t counter_nonsecure;
		} codec_mm;
		/**
		 * @total: max available capacity size
		 */
		struct {
			__u32 total;
		} capacity;
	} d;
};

struct module_debug_node {
	struct list_head list;
	char *module;
	debug_callback callback;
};

struct error_info_node {
	struct list_head list;
	char *info;
};

static int resman_debug = 1;
static int preempt_timeout_ms = 2500;
static struct list_head sessions_head;
static struct list_head resources_head;
static struct list_head debug_head;
static struct list_head error_info_head;
static DEFINE_MUTEX(sessions_lock);
static DEFINE_MUTEX(resource_lock);
static DEFINE_MUTEX(debug_lock);
static DEFINE_MUTEX(error_info_lock);
static DEFINE_MUTEX(resman_cb_ops_mutex);
static LIST_HEAD(g_cb_ops_list);

static dev_t resman_devno;
static int sess_id = 1;
static struct cdev *resman_cdev;
static struct class *resman_class;
static char *resman_configs;
static char *debug_info;

int pipe_kpi_debug;
EXPORT_SYMBOL(pipe_kpi_debug);
module_param(pipe_kpi_debug, int, 0644);

module_param(resman_debug, int, 0644);
module_param(preempt_timeout_ms, int, 0644);

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (resman_debug >= (level))					\
			pr_info("resman: " fmt, ## arg);	\
	} while (0)

static int resman_register_default_query_ops(struct resman_cb_param_any_t q)
{
	dprintk(0, "resman: default_query_resource_cb not registered\n");
	return RESMAN_DEFAULIT_EXPECT_SIZE_FOR_PLAYBACK;
}

static const char *const resman_valid_devices[] = {
	RESMAN_DEV_VDEC,
	RESMAN_DEV_DI,
	RESMAN_DEV_PP,
};

static bool resman_is_valid_device_name(const char *name)
{
	int i = 0;

	if (!name)
		return false;
	for (i = 0; i < ARRAY_SIZE(resman_valid_devices); ++i) {
		if (strncmp(resman_valid_devices[i], name, RESMAN_LEN) == 0)
			return true;
	}
	dprintk(0, "resman:  device name is not valid\n");
	return false;
}

/**
 * resman_parser_kv() - parse key value pair
 *
 * @str: input string, must rw
 * @k: key name
 * @v: value to store
 *
 * parse key value pair like "foo:1"
 *
 * Return: true for success of false
 */
static inline bool resman_parser_kv(char *str, const char *k, __u32 *v)
{
	bool ret = false;
	char *sk, *sv;

	sk = strsep(&str, ":");
	sv = str;
	if (k && v && sk && sv && !strcmp(sk, k) && !kstrtou32(sv, 0, v))
		ret = true;
	if (!ret)
		dprintk(2, "parse %s  %s\n", k, str);
	return ret;
}

static struct resman_resource *resman_find_resource_by_id(int id)
{
	struct resman_resource *ret = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &resources_head) {
		ret = list_entry(pos, struct resman_resource, list);
		if (ret->id == id)
			return ret;
	}
	return NULL;
}

static struct resman_resource *resman_find_resource_by_name(const char *name)
{
	struct resman_resource *ret = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &resources_head) {
		ret = list_entry(pos, struct resman_resource, list);
		if (!strcmp(name, ret->name))
			return ret;
	}
	return NULL;
}

static struct resman_node *resman_find_node_by_resource_id
		(struct resman_session *sess, int id)
{
	struct resman_node *ret = NULL;
	struct resman_resource *resource = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &sess->resources) {
		ret = list_entry(pos, struct resman_node, rlist);
		resource = (struct resman_resource *)ret->resource_ptr;
		if (resource->id == id)
			return ret;
	}
	return NULL;
}

static void resman_find_appname_by_resource_name(const char *name, char *appname)
{
	struct list_head *pos1, *tmp1;
	struct list_head *pos2, *tmp2;
	struct resman_resource *resource;
	struct resman_session *sess;
	struct resman_node *node;

	mutex_lock(&resource_lock);
	mutex_lock(&sessions_lock);
	list_for_each_safe(pos2, tmp2, &sessions_head) {
		sess = list_entry(pos2, struct resman_session, list);
		list_for_each_safe(pos1, tmp1, &sess->resources) {
			node = list_entry(pos1, struct resman_node, rlist);
			resource = node->resource_ptr;
			if (resource && appname && !strcmp(name, resource->name)) {
				dprintk(2, "%d acquired app_name %s\n", sess->id, sess->app_name);
				strncpy(appname, sess->app_name, RESMAN_STR_MAX_SIZE);
				break;
			}
		}
	}
	mutex_unlock(&sessions_lock);
	mutex_unlock(&resource_lock);
}

#define APPEND_ATTR_BUF(format, args...) { \
	size += snprintf(buf + size, PAGE_SIZE - size, format, args); \
}

/**
 * append_to_buf_safe() - Safely append formatted text to a dynamically
 *                        allocated buffer with automatic growth.
 *
 * @pbuf:       Pointer to the buffer pointer (will be reallocated as needed)
 * @psize:      Pointer to the current number of bytes written (accumulated)
 * @pbuf_size:  Pointer to the current total allocated buffer size
 * @fmt:        printf-style format string
 * @...:        Variable arguments corresponding to @fmt
 *
 * This function appends formatted output to a string buffer. If the remaining
 * space in the buffer is insufficient, it automatically reallocates a larger
 * buffer using krealloc() and retries the operation. The buffer pointer and
 * its size trackers (*pbuf and *pbuf_size) are updated if reallocation occurs.
 *
 * Return:
 * * 0 on success
 * * -ENOMEM if memory allocation or reallocation fails
 * * -EINVAL if invalid arguments are provided
 *
 * Example usage:
 *
 *	size_t size = 0;
 *	size_t buf_size = PAGE_SIZE;
 *	char *buf = kzalloc(buf_size, GFP_KERNEL);
 *
 *	append_to_buf_safe(&buf, &size, &buf_size, "value: %d\n", val);
 *	append_to_buf_safe(&buf, &size, &buf_size, "string: %s\n", name);
 *
 *	pr_info("%s", buf);
 *	kfree(buf);
 */

static int append_to_buf_safe(char **pbuf, size_t *psize, size_t *pbuf_size,
			      const char *fmt, ...)
{
	va_list args;
	int needed;
	int avail;
	char *new_buf;
	size_t new_size;
	char *buf;

	if (!pbuf || !psize || !pbuf_size || !fmt)
		return -EINVAL;
	buf = *pbuf;
retry:
	avail = *pbuf_size - *psize;
	va_start(args, fmt);
	needed = vsnprintf(buf + *psize, avail, fmt, args);
	va_end(args);

	if (needed >= avail) {
		new_size = max(*pbuf_size * 2, *pbuf_size + needed + 1);
		new_buf = krealloc(*pbuf, new_size, GFP_KERNEL);
		if (!new_buf) {
			dprintk(0, " OOM, truncating output\n");
			return -ENOMEM;
		}
		*pbuf = new_buf;
		*pbuf_size = new_size;
		buf = new_buf;
		goto retry;
	}

	*psize += needed;
	return 0;
}

static ssize_t resman_usage_dump(void)
{
	struct list_head *pos1, *tmp1;
	struct list_head *pos2, *tmp2;
	struct resman_resource *resource;
	struct resman_session *sess;

	size_t size = 0;
	size_t buf_size = PAGE_SIZE;
	char *buf = NULL;
	int i, j;

	mutex_lock(&resource_lock);
	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf) {
		mutex_unlock(&resource_lock);
		return 0;
	}

	j = 0;
	list_for_each_safe(pos1, tmp1, &resources_head) {
		resource = list_entry(pos1, struct resman_resource, list);
		for (i = 0; i < j; i++)
			append_to_buf_safe(&buf, &size, &buf_size, "%c ", '|');
		append_to_buf_safe(&buf, &size, &buf_size,
				   "%s %d\n", resource->name, resource->value);
		j++;
	}

	for (i = 0; i < j; i++)
		append_to_buf_safe(&buf, &size, &buf_size, "%c ", '|');
	append_to_buf_safe(&buf, &size, &buf_size, "%-20s", "name");
	append_to_buf_safe(&buf, &size, &buf_size, "%-10s", "status");
	append_to_buf_safe(&buf, &size, &buf_size, "%-16s\n", "proc/PID/TID");

	mutex_lock(&sessions_lock);
	list_for_each_safe(pos2, tmp2, &sessions_head) {
		sess = list_entry(pos2, struct resman_session, list);
		list_for_each_safe(pos1, tmp1, &resources_head) {
			resource = list_entry(pos1, struct resman_resource, list);
			if (resman_find_node_by_resource_id(sess, resource->id))
				append_to_buf_safe(&buf, &size, &buf_size, "%-2c", 'x');
			else
				append_to_buf_safe(&buf, &size, &buf_size, "%-2c", ' ');
		}
		append_to_buf_safe(&buf, &size, &buf_size, "%-20s", sess->app_name);
		switch (sess->status) {
		case RESMAN_STATUS_INITED:
			append_to_buf_safe(&buf, &size, &buf_size, "%-10s", "inited");
			break;
		case RESMAN_STATUS_ACQUIRED:
			append_to_buf_safe(&buf, &size, &buf_size, "%-10s", "acquired");
			break;
		case RESMAN_STATUS_PREEMPTED:
			append_to_buf_safe(&buf, &size, &buf_size, "%-10s", "preempted");
			break;
		default:
			append_to_buf_safe(&buf, &size, &buf_size, "%-10s", "unknown");
			break;
		}
		append_to_buf_safe(&buf, &size, &buf_size,
				   "%s/%d/%d\n", sess->comm, sess->tgid, sess->pid);
	}
	mutex_unlock(&sessions_lock);
	mutex_unlock(&resource_lock);

	append_to_buf_safe(&buf, &size, &buf_size, "%s", "\n");

	if (buf) {
		size_t len = strlen(buf);
		size_t offset = 0, remain = 0, chunk = 0, print_len = 0;
		size_t  remain_in_chunk = 0, advanced = 0;
		char *found = NULL, *search = NULL, *p = NULL;
		char tmp[RESMAN_MAX_PRINT_LEN + 1];

		while (offset < len) {
			remain = len - offset;
			chunk = min_t(size_t, remain, RESMAN_MAX_PRINT_LEN);
			print_len = chunk;
			found = NULL;
			search = buf + offset;
			remain_in_chunk = chunk;

			while (remain_in_chunk > 0) {
				p = memchr(search, '\n', remain_in_chunk);
				if (!p)
					break;
				found = p;
				advanced = p - (buf + offset) + 1;
				if (advanced >= chunk)
					break;
				remain_in_chunk = chunk - advanced;
				search = p + 1;
			}

			if (found)
				print_len = found - (buf + offset) + 1;

			memcpy(tmp, buf + offset, print_len);
			tmp[print_len] = '\0';
			pr_info("%s", tmp);

			offset += print_len;
		}
	}

	kfree(buf);
	return size;
}

static int resman_count_codec_mm_session(bool secure)
{
	struct resman_resource *resource;
	int count = 0;

	resource = resman_find_resource_by_id(RESMAN_ID_CODEC_MM);
	if (!resource)
		return 0;

	if (secure)
		count = atomic_read(&resource->d.codec_mm.counter_secure);
	else
		count = atomic_read(&resource->d.codec_mm.counter_nonsecure);

	return count;
}

static struct counter_res_state_list *resman_counter_res_list_init(struct resman_resource *resource,
								int size)
{
	int i = 0;

	struct counter_res_state_list *counter_list =
		kcalloc(1, sizeof(struct counter_res_state_list), GFP_KERNEL);
	if (!counter_list)
		return NULL;

	counter_list->items =
		kcalloc(size, sizeof(struct counter_res_state), GFP_KERNEL);
	if (!counter_list->items) {
		kfree(counter_list);
		return NULL;
	}
	counter_list->size = size;
	dprintk(4, "list size %d res id %d\n", size, resource->id);

	for (i = 0; i < size; i++) {
		counter_list->items[i].index = i;
		counter_list->items[i].state = 0;
		counter_list->items[i].ssid = -1;
	}
	return counter_list;
}

static int resman_counter_res_list_acquire_item(struct counter_res_state_list *counter_list,
			int ssid)
{
	int i = 0;
	int index = -1;

	if (!counter_list)
		return -1;
	for (i = 0; i < counter_list->size; i++) {
		if (counter_list->items[i].state == 0) {
			counter_list->items[i].state = 1;
			counter_list->items[i].ssid = ssid;
			index = counter_list->items[i].index;
			break;
		}
	}
	return index;
}

static void resman_counter_res_list_release_item(struct counter_res_state_list *counter_list,
			int index, int ssid)
{
	int i = 0;

	if (!counter_list || ssid <= 0)
		return;
	for (i = 0; i < counter_list->size; i++) {
		if (counter_list->items[i].ssid == ssid) {
			counter_list->items[i].state = 0;
			counter_list->items[i].ssid = -1;
			break;
		}
	}
}

static bool resman_acquire_resource(struct resman_session *sess,
				    struct resman_resource *resource,
				    bool preempt,
				    __u32 timeout,
				    const char *arg_ro)
{
	bool ret = false;
	struct resman_node *node = NULL;
	char *arg_rw = NULL;

	if (arg_ro)
		arg_rw = kstrdup(arg_ro, GFP_KERNEL);

	node = kzalloc(sizeof(*node), GFP_KERNEL);

	if (atomic_read(&resource->pending_acquire) > 0)
		wait_event_interruptible(resource->wq_acquire,
				atomic_read(&resource->pending_acquire) == 0);

	mutex_lock(&resource->lock);
	dprintk(2, "%d acquire [%s], preempt %s\n",
		sess->id,
		resource->name,
		preempt ? "yes" : "no");
	if (node && resource->acquire(sess, resource, node, preempt, timeout, arg_rw)) {
		list_add_tail(&node->rlist, &sess->resources);
		list_add_tail(&node->slist, &resource->sessions);
		init_completion(&node->pending_release);
		node->session_ptr = (void *)sess;
		node->resource_ptr = (void *)resource;
		ret = true;
		sess->status = RESMAN_STATUS_ACQUIRED;
		dprintk(2, "%d acquire [%s] success\n",
			sess->id,
			resource->name);
	} else {
		dprintk(1, "%d acquire [%s] failed\n",
			sess->id, resource->name);
	}
	mutex_unlock(&resource->lock);
	kfree(arg_rw);
	if (!ret || resman_debug >= 5) {
		if (resource->type == RESMAN_TYPE_CODEC_MM) {
			dump_mem_infos_external();
			codec_mm_dbuf_walk_external();
		}
		resman_usage_dump();
	}
	return ret;
}

static void resman_release_resource(struct resman_node *node)
{
	struct resman_session *sess = node->session_ptr;
	struct resman_resource *resource = node->resource_ptr;

	if (!mutex_trylock(&resource->lock)) {
		while (!completion_done(&node->pending_release))
			complete(&node->pending_release);
		mutex_lock(&resource->lock);
	}

	resource->release(sess, resource, node);
	list_del(&node->rlist);
	list_del(&node->slist);
	dprintk(2, "%d release [%s]\n",
		sess->id,
		resource->name);
	wake_up_interruptible_all(&resource->wq_release);
	mutex_unlock(&resource->lock);
	kfree(node);
}

static struct resman_session *resman_open_session(void)
{
	struct resman_session *sess = NULL;

	sess = kzalloc(sizeof(*sess), GFP_KERNEL);
	if (!sess)
		return NULL;

	INIT_LIST_HEAD(&sess->resources);
	INIT_LIST_HEAD(&sess->events);
	mutex_init(&sess->lock);
	init_waitqueue_head(&sess->wq_event);
	strcpy(sess->comm, current->comm);
	sess->tgid = current->tgid;
	sess->pid = current->pid;
	sess->status = RESMAN_STATUS_INITED;
	sess->id = sess_id++;
	sess->param_set = param_set_create();
	mutex_lock(&sessions_lock);
	list_add_tail(&sess->list, &sessions_head);
	mutex_unlock(&sessions_lock);
	return sess;
}

static void resman_close_session(struct resman_session *sess)
{
	struct list_head *pos, *tmp;
	struct resman_node *node;
	struct resman_event *event;

	if (!sess)
		return;

	mutex_lock(&sess->lock);
	wake_up_interruptible_all(&sess->wq_event);
	list_for_each_safe(pos, tmp, &sess->resources) {
		node = list_entry(pos, struct resman_node, rlist);
		dprintk(3, "%d release\n", sess->id);
		resman_release_resource(node);
	}
	list_for_each_safe(pos, tmp, &sess->events) {
		event = list_entry(pos, struct resman_event, list);
		list_del(&event->list);
		kfree(event);
	}
	if (sess->param_set) {
		dprintk(5, "%d param_set destroy\n", sess->id);
		param_set_destroy(sess->param_set);
		sess->param_set = NULL;
	}
	mutex_unlock(&sess->lock);
	mutex_lock(&sessions_lock);
	list_del(&sess->list);
	mutex_unlock(&sessions_lock);
	kfree(sess);
}

static void resman_send_event(struct resman_session *sess, __u32 type)
{
	struct resman_event *event = NULL;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event)
		return;

	event->type = type;
	list_add_tail(&event->list, &sess->events);
}

static bool resman_preempt_session(struct resman_session *curr,
				   struct resman_node *node)
{
	bool ret = false;
	bool need_unlock = true;
	struct resman_session *sess;

	if (!node)
		goto beach;

	sess = node->session_ptr;
	if (sess->status != RESMAN_STATUS_ACQUIRED)
		goto beach;

	if (!mutex_trylock(&sess->lock)) {
		/*other session may waiting to release this resource*/
		if (wait_for_completion_interruptible_timeout(&node->pending_release,
				usecs_to_jiffies(10000)) <= 0)
			goto beach;
		need_unlock = false;
	}
	dprintk(1, "%d preempting %d %s\n",
		curr->id,
		sess->id,
		sess->app_name);
	sess->status = RESMAN_STATUS_PREEMPTED;
	resman_send_event(sess, RESMAN_EVENT_PREEMPT);
	wake_up_interruptible(&sess->wq_event);
	if (need_unlock)
		mutex_unlock(&sess->lock);
	ret = true;
beach:
	return ret;
}

/**
 * resman_counter_preempt() - preempt for counter type
 *
 * @curr: current session data
 * @resource: resource data
 * Return: true for success or false
 */
static bool resman_counter_preempt(struct resman_session *curr,
				   struct resman_resource *resource)
{
	bool ret = false;
	struct resman_node *node, *selected = NULL;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	int lowestprio = curr->prio;

	list_for_each_safe(pos, tmp, &resource->sessions) {
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (sess->status == RESMAN_STATUS_PREEMPTED && sess->prio >= curr->prio) {
			selected = node;
			dprintk(2, "%d preempt prio%d\n", sess->id, sess->prio);
			break;
		}
	}
	/*find the lowest prio*/
	if (!selected) {
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->prio > lowestprio) {
				selected = node;
				lowestprio = sess->prio;
			}
		}
	}
	/*find the oldest same prio*/
	if (!selected) {
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->prio == lowestprio) {
				selected = node;
				break;
			}
		}
	}
	if (!selected)
		dprintk(2, "%d fail to find node to preempt\n", sess->id);
	else
		dprintk(2, "%d preempt node prio:%d\n", sess->id, lowestprio);

	if (resman_preempt_session(curr, selected))
		ret = true;

	return ret;
}

/**
 * resman_counter_acquire() - acquire counter resource
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 *
 * acquire a counter resource,
 * Return: true for success or false
 */
static bool resman_counter_acquire(struct resman_session *sess,
				   struct resman_resource *resource,
				   struct resman_node *node,
				   bool preempt,
				   __u32 timeout,
				   char *arg)
{
	bool ret = false;
	long remain;

	if (resource->value >= resource->d.counter.avail) {
		if (preempt) {
			if (!timeout)
				timeout = preempt_timeout_ms;
			if (!resman_counter_preempt(sess, resource))
				timeout = 0;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			remain = wait_event_interruptible_timeout(resource->wq_release,
					(resource->value <
						resource->d.counter.avail),
					msecs_to_jiffies(timeout));
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id,
				remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	}
	if (resource->value < resource->d.counter.avail) {
		resource->value++;
		resman_counter_res_list_acquire_item(resource->d.counter.counter_list, sess->id);
		ret = true;
	}
	return ret;
}

/**
 * resman_counter_release() - release counter resource
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_counter_release(struct resman_session *sess,
				   struct resman_resource *resource,
				   struct resman_node *node)
{
	if (resource->value > 0) {
		resource->value--;
		resman_counter_res_list_release_item(resource->d.counter.counter_list,
		resource->value, sess->id);
	}
}

/**
 * resman_toggle_acquire() - acquire toggle resource
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_toggle_acquire(struct resman_session *sess,
				  struct resman_resource *resource,
				  struct resman_node *node,
				  bool preempt,
				  __u32 timeout,
				  char *arg)
{
	bool ret = false;

	if (arg) {
		switch (*arg) {
		case '+':
			if (resource->value >= 0) {
				resource->value++;
				node->s.toggle.type = '+';
				ret = true;
			}
			break;
		case '-':
			if (resource->value <= 0) {
				resource->value--;
				node->s.toggle.type = '-';
				ret = true;
			}
			break;
		default:
			break;
		}
	}
	return ret;
}

/**
 * resman_toggle_release() - resman_toggle_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_toggle_release(struct resman_session *sess,
				  struct resman_resource *resource,
				  struct resman_node *node)
{
	switch (node->s.toggle.type) {
	case '+':
		if (resource->value > 0)
			resource->value--;
		break;
	case '-':
		if (resource->value < 0)
			resource->value++;
		break;
	default:
		break;
	}
	node->s.toggle.type = 0;
}

/**
 * resman_tvp_preempt() - resman_tvp_preempt
 *
 * @curr: current session data
 * Return: true for success or false
 */
static bool resman_tvp_preempt(struct resman_session *curr)
{
	bool ret = false;
	struct resman_resource *resource;
	struct resman_node *node, *selected = NULL;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	int lowestprio = curr->prio;

	resource = resman_find_resource_by_id(RESMAN_ID_CODEC_MM);
	if (!resource)
		return false;

	/*find the lowest non-secure prio*/
	list_for_each_safe(pos, tmp, &resource->sessions) {
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (!node->s.codec_mm.secure && sess->prio > lowestprio)
			lowestprio = sess->prio;
	}

	/*find the oldest non-secure decoder instance*/
	list_for_each_safe(pos, tmp, &resource->sessions) {
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (!node->s.codec_mm.secure && sess->prio >= lowestprio) {
			selected = node;
			break;
		}
	}

	if (resman_preempt_session(curr, selected))
		ret = true;

	return ret;
}

/**
 * resman_tvp_acquire() - resman_tvp_acquire
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_tvp_acquire(struct resman_session *sess,
			       struct resman_resource *resource,
			       struct resman_node *node,
			       bool preempt,
			       __u32 timeout,
			       char *arg)
{
	bool ret = false;
	bool again = false;
	long remain;
	int size = 0;
	int flags = 1;
	int count;

	if (!resman_parser_kv(arg, "size", (__u32 *)&size)) {
		if (!strcmp(arg, "uhd"))
			flags = 2;
	}

	do {
		count = resman_count_codec_mm_session(false);
		if (!codec_mm_enable_tvp(size, flags)) {
			resource->value++;
			ret = true;
			break;
		}
		/*If TVP alloc failed, reclaim non-secure decoder instance*/
		if (!preempt || !count)
			break;

		if (!timeout)
			timeout = preempt_timeout_ms;

		if (!resman_tvp_preempt(sess)) {
			again = false;
			timeout = 0;
		} else {
			again = true;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			remain = wait_event_interruptible_timeout(resource->wq_release,
					(resman_count_codec_mm_session(false)
							< count),
					msecs_to_jiffies(timeout));
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id,
				remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	} while (again);

	return ret;
}

/**
 * resman_tvp_release() - resman_tvp_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_tvp_release(struct resman_session *sess,
			       struct resman_resource *resource,
			       struct resman_node *node)
{
	if (resource->value > 0) {
		codec_mm_disable_tvp();
		resource->value--;
	}
}

/**
 * resman_codec_mm_preempt() - resman_codec_mm_preempt
 *
 * @curr: current session data
 * @resource: resource data
 * @score: acquired score
 * @secure: secure or not
 * Return: true for success or false
 */
static bool resman_codec_mm_preempt(struct resman_session *curr,
				    struct resman_resource *resource,
				    __u32 score,
				    bool secure)
{
	bool ret = false;
	int secure_count;
	struct resman_node *node, *selected;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	__u32 new_score;
	int lowestprio = curr->prio;


	secure_count = atomic_read(&resource->d.codec_mm.counter_secure);

	/*Reclaim secure instance if exceed limitation*/
	list_for_each_safe(pos, tmp, &resource->sessions) {
		if (!secure || secure_count < resource->d.codec_mm.secure)
			break;
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (node->s.codec_mm.secure) {
			if (resman_preempt_session(curr, node))
				ret = true;
			secure_count--;
		}
	}

	do {
		selected = NULL;
		/*Calculate the available score*/
		new_score = resource->value;
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_PREEMPTED)
				new_score -= node->s.codec_mm.score;
		}
		if (new_score + score <= resource->d.codec_mm.total)
			break;
		/*find the lowest prio*/
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_ACQUIRED && sess->prio > lowestprio) {
				selected = node;
				lowestprio = sess->prio;
			}
		}
		/*find the oldest same prio*/
		if (!selected) {
			list_for_each_safe(pos, tmp, &resource->sessions) {
				node = list_entry(pos, struct resman_node, slist);
				sess = node->session_ptr;
				if (sess->status == RESMAN_STATUS_ACQUIRED &&
					sess->prio == lowestprio) {
					dprintk(2, "%d preempt prio:%d\n", sess->id, sess->prio);
					selected = node;
					break;
				}
			}
		}
		if (!selected) {
			dprintk(2, "%d fail to find node to preempt\n", sess->id);
			break;
		}
		dprintk(2, "%d preempt node prio:%d\n", sess->id, lowestprio);
		if (resman_preempt_session(curr, selected)) {
			new_score -= selected->s.codec_mm.score;
			lowestprio = curr->prio;
			/*if not enough need preempt more*/
			ret = true;
		}
	} while (selected &&
		(new_score + score > resource->d.codec_mm.total));

	return ret;
}

static bool resman_codec_mm_enough(struct resman_session *sess,
				struct resman_resource *resource,
				__u32 score,
				bool secure)
{
	bool enough = true;

	if (!secure && (codec_mm_get_free_size() >> 20 < score)) {
		enough = false;
		dprintk(2, "%d free size 0x%x\n", sess->id, codec_mm_get_free_size());
		if (resman_debug)
			dump_mem_infos_external();
	} else if (secure && ((codec_mm_get_tvp_free_size() + codec_mm_get_free_size()) >> 20)
			< score) {
		enough = false;
		dprintk(2, "%d free size 0x%x\n", sess->id, codec_mm_get_tvp_free_size() +
			+ codec_mm_get_free_size());
		if (resman_debug)
			dump_mem_infos_external();
	}
	return enough;
}

/**
 * resman_codec_mm_acquire() - resman_codec_mm_acquire
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_codec_mm_acquire(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node,
				    bool preempt,
				    __u32 timeout,
				    char *arg)
{
	bool ret = false;
	long remain;
	__u32 score = resource->d.codec_mm.fhd;
	bool secure = false;
	char *opt;
	__u32 slicetimeout = 300, attempt = 0, max_attempts = 10;

	while ((opt = strsep(&arg, ","))) {
		if (!strncmp(opt, "size", 4)) {
			if (!resman_parser_kv(opt, "size", &score))
				dprintk(1, "%d default fhd size\n", sess->id);
		}
		else if (!strcmp(opt, "single"))
		/*single mode, 64M codec mm size is enough*/
			score = RESMAN_SINGLE_SIZE;
		else if (!strcmp(opt, "uhd"))
			score = resource->d.codec_mm.uhd;
		else if (!strcmp(opt, "secure"))
			secure = true;
	}
	dprintk(2, "%d score %d secure %s\n",
		sess->id,
		score,
		secure ? "yes" : "no");

	if (!resman_codec_mm_enough(sess, resource, score, secure)) {
		if (preempt) {
			if (!timeout)
				timeout = preempt_timeout_ms;
			if (!resman_codec_mm_preempt(sess, resource,
						     score, secure))
				timeout = 0;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			max_attempts = timeout / slicetimeout;
			while (attempt < max_attempts) {
				dprintk(1, "%d max_attempts %d attempt %d\n",
					sess->id, max_attempts, attempt);
				remain = wait_event_interruptible_timeout(resource->wq_release,
					resman_codec_mm_enough(sess, resource,
							score,
							secure),
				msecs_to_jiffies(slicetimeout));

				if (remain > 0) {
					// Condition became true
					break;
				} else if (remain == 0) {
					// Timeout occurred
					attempt++;
				} else {
					// Interrupted by a signal
					break;
				}
			}
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id,
				remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	}

	if (resman_codec_mm_enough(sess, resource, score, secure)) {
		node->s.codec_mm.score = score;
		node->s.codec_mm.secure = secure;
		resource->value += score;
		if (secure)
			atomic_inc(&resource->d.codec_mm.counter_secure);
		else
			atomic_inc(&resource->d.codec_mm.counter_nonsecure);
		ret = true;
	}
	return ret;
}

/**
 * resman_codec_mm_release() - resman_codec_mm_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_codec_mm_release(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node)
{
	if (node->s.codec_mm.secure)
		atomic_dec(&resource->d.codec_mm.counter_secure);
	else
		atomic_dec(&resource->d.codec_mm.counter_nonsecure);
	if (resource->value >= node->s.codec_mm.score)
		resource->value -= node->s.codec_mm.score;
}

/**
 * void resman_codec_mm_probe() - probe codec mm parameters
 *
 * @resource: resource context
 */
static void resman_codec_mm_probe(struct resman_resource *resource)
{
	int total_bytes = 0, tvp_fhd = 0, tvp_uhd = 0;

	codec_mm_get_default_tvp_size(&tvp_fhd, &tvp_uhd);
	total_bytes = codec_mm_get_total_size();
	if (total_bytes >= tvp_fhd + tvp_uhd)
		resource->d.codec_mm.secure = 2;
	else
		resource->d.codec_mm.secure = 1;
	resource->d.codec_mm.total = total_bytes >> 20;
	resource->d.codec_mm.uhd = tvp_uhd >> 20;
	resource->d.codec_mm.fhd = tvp_fhd >> 20;
}

static int resman_codec_mm_update_size(struct resman_session *sess, int cur_size)
{
	struct resman_node *node = NULL;
	struct resman_resource *resource = NULL;
	struct list_head *pos, *tmp;
	int old_size = 0;

	mutex_lock(&sess->lock);
	list_for_each_safe(pos, tmp, &sess->resources) {
		node = list_entry(pos, struct resman_node, rlist);
		resource = (struct resman_resource *)node->resource_ptr;
		if (resource->id == RESMAN_ID_CODEC_MM) {
			if (node->s.codec_mm.score != cur_size) {
				old_size = node->s.codec_mm.score;
				node->s.codec_mm.score = cur_size;
				dprintk(4, "%d update score size %d to %d\n",
					sess->id, old_size, cur_size);
				if (old_size < cur_size)
					resource->value += (cur_size - old_size);
				else
					resource->value -= (old_size - cur_size);
			}
			mutex_unlock(&sess->lock);
			return 1;
		}
	}
	mutex_unlock(&sess->lock);
	return 0;
}

static bool resman_capacity_enough(struct resman_resource *resource,
				   __u32 size)
{
	bool enough = true;

	if ((resource->value + size) > resource->d.capacity.total)
		enough = false;
	dprintk(2, "[%s] value 0x%x score 0x%x total 0x%x\n",
			resource->name, resource->value,
			size, resource->d.capacity.total);
	return enough;
}

/**
 * resman_capacity_preempt() - resman_capacity_preempt
 *
 * @curr: current session data
 * @resource: resource data
 * @size: need resman to release resource size
 * Return: true for success or false
 */

static bool resman_capacity_preempt(struct resman_session *curr,
				    struct resman_resource *resource,
				    __u32 size)
{
	bool ret = false;
	struct resman_node *node, *selected;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	__u32 used_size;
	int lowestprio = curr->prio;

	do {
		selected = NULL;
		/*Calculate the available size*/
		used_size = resource->value;
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_PREEMPTED)
				used_size -= node->s.capacity.use;
		}
		dprintk(2, "used_size 0x%x size 0x%x\n", used_size, size);
		if ((used_size + size) <= resource->d.capacity.total)
			break;

		/*find the lowest prio*/
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_ACQUIRED && sess->prio > lowestprio) {
				selected = node;
				lowestprio = sess->prio;
			}
		}
		/*find the oldest same prio*/
		if (!selected) {
			list_for_each_safe(pos, tmp, &resource->sessions) {
				node = list_entry(pos, struct resman_node, slist);
				sess = node->session_ptr;
				if (sess->status == RESMAN_STATUS_ACQUIRED &&
					sess->prio == lowestprio) {
					selected = node;
					break;
				}
			}
		}
		if (!selected) {
			dprintk(2, "fail to find node to preempt\n");
			break;
		}
		dprintk(2, "preempt node prio:%d\n", lowestprio);
		if (resman_preempt_session(curr, selected)) {
			used_size -= selected->s.capacity.use;
			lowestprio = curr->prio;
			/*if not enough need preempt more*/
			ret = true;
		}
	} while (selected &&
		(used_size + size > resource->d.capacity.total));

	return ret;
}

/**
 * resman_capacity_acquire() - resman_capacity_acquire
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_capacity_acquire(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node,
				    bool preempt,
				    __u32 timeout,
				    char *arg)
{
	bool ret = false;
	long remain;
	__u32 size = 0;
	char *opt;

	while ((opt = strsep(&arg, ","))) {
		if (!strncmp(opt, "size", 4))
			if (!resman_parser_kv(opt, "size", &size))
				return ret;
	}
	if (size <= 0) {
		dprintk(0, "%d size %d parameter format:size:X!\n", sess->id, size);
		return ret;
	}

	if (!resman_capacity_enough(resource, size)) {
		if (preempt) {
			if (!timeout)
				timeout = preempt_timeout_ms;
			if (!resman_capacity_preempt(sess, resource, size))
				timeout = 0;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			remain =
			wait_event_interruptible_timeout(resource->wq_release,
				resman_capacity_enough(resource, size),
				msecs_to_jiffies(timeout));
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id, remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	}

	if (resman_capacity_enough(resource, size)) {
		node->s.capacity.use = size;
		resource->value += size;
		ret = true;
	}
	return ret;
}

/**
 * resman_codec_mm_release() - resman_codec_mm_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_capacity_release(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node)
{
	if (resource->value >= node->s.capacity.use)
		resource->value -= node->s.capacity.use;
}

static bool resman_create_resource(const char *name,
				   __u32 type,
				   char *arg)
{
	struct resman_resource *resource = NULL, *restmp = NULL;
	char *opt;
	int i;
	char r0 = 0;
	char r1 = 0;
	char r2 = 0;
	char r3 = 0;

	resource = kzalloc(sizeof(*resource), GFP_KERNEL);
	if (!resource || !name)
		goto error;

	resource->id = -1;
	for (i = 0; i < ARRAY_SIZE(resources_map) && resources_map[i].name;
			i++) {
		dprintk(3, "map: [%s]\n", resources_map[i].name);
		if (!strcmp(resources_map[i].name, name)) {
			restmp = resman_find_resource_by_id(resources_map[i].id);
			if (restmp) {
				dprintk(3, "notice: [%s] exist\n", name);
				kfree(resource);
				resource = restmp;
			} else {
				resource->id = resources_map[i].id;
				strncpy(resource->name, resources_map[i].name,
				sizeof(resource->name));
				resource->name[sizeof(resource->name) - 1] = 0;
			}
			break;
		}
	}
	if (i >= ARRAY_SIZE(resources_map)) {
		if (strlen(name) < 4) {
			dprintk(0, "At least 4 letters are required for resource name\n");
			goto error;
		}
		r0 = name[0];
		r1 = name[1];
		r2 = name[2];
		r3 = name[3];
		resource->id = (r0 << 24) | (r1 << 16) | (r2 << 8) | r3;
		restmp = resman_find_resource_by_id(resource->id);
		if (restmp) {
			dprintk(3, "notice: [%s] exist\n", name);
			kfree(resource);
			resource = restmp;
		} else {
			if (strlen(name) < 32)
				strncpy(resource->name, name, strlen(name));
			dprintk(3, "ext resource id[%d] name %s\n",
				resource->id, resource->name);
		}
	}
	if (resource->id < 0) {
		dprintk(0, "error, [%s] invalid\n", name);
		goto error;
	}

	switch (type) {
	default:
		dprintk(0, "error, unknown type\n");
		goto error;
	case RESMAN_TYPE_COUNTER:
		resource->acquire = resman_counter_acquire;
		resource->release = resman_counter_release;

		if (!resman_parser_kv(arg, "avail",
				      &resource->d.counter.avail)) {
			goto error;
		}
		resource->d.counter.counter_list =
			resman_counter_res_list_init(resource, resource->d.counter.avail);
		break;
	case RESMAN_TYPE_TOGGLE:
		resource->acquire = resman_toggle_acquire;
		resource->release = resman_toggle_release;
		break;
	case RESMAN_TYPE_TVP:
		resource->acquire = resman_tvp_acquire;
		resource->release = resman_tvp_release;
		break;
	case RESMAN_TYPE_CODEC_MM:
		resource->acquire = resman_codec_mm_acquire;
		resource->release = resman_codec_mm_release;

		opt = strsep(&arg, ",");
		if (!resman_parser_kv(opt, "total",
				      &resource->d.codec_mm.total)) {
			goto error;
		}
		if (resource->d.codec_mm.total) {
			opt = strsep(&arg, ",");
			if (!resman_parser_kv(opt, "uhd",
					      &resource->d.codec_mm.uhd)) {
				goto error;
			}
			opt = strsep(&arg, ",");
			if (!resman_parser_kv(opt, "fhd",
					      &resource->d.codec_mm.fhd)) {
				goto error;
			}
			opt = strsep(&arg, ",");
			if (!resman_parser_kv(opt, "secure",
					      &resource->d.codec_mm.secure)) {
				goto error;
			}
		} else {
			resman_codec_mm_probe(resource);
		}
		atomic_set(&resource->d.codec_mm.counter_secure, 0);
		atomic_set(&resource->d.codec_mm.counter_nonsecure, 0);
		break;
	case RESMAN_TYPE_CAPACITY_SIZE:
		resource->acquire = resman_capacity_acquire;
		resource->release = resman_capacity_release;

		opt = strsep(&arg, ",");
		if (!resman_parser_kv(opt, "total",
				&resource->d.capacity.total)) {
			goto error;
		}
		break;
	}
	resource->type = type;
	if (!restmp) {//resource exist just need update the parameter
		INIT_LIST_HEAD(&resource->sessions);
		mutex_init(&resource->lock);
		list_add_tail(&resource->list, &resources_head);
		init_waitqueue_head(&resource->wq_release);
		init_waitqueue_head(&resource->wq_acquire);
		atomic_set(&resource->pending_acquire, 0);
	}
	return true;
error:
	kfree(resource);
	return false;
}

static void resman_destroy_resource(struct resman_resource *resource)
{
	if (list_empty(&resource->sessions)) {
		list_del(&resource->list);
		kfree(resource);
	}
}

static void all_resource_uninit(void)
{
	struct resman_resource *resource;
	struct resman_node *node;
	struct list_head *pos, *tmp;
	struct list_head *pos1, *tmp1;

	mutex_lock(&resource_lock);
	kfree(resman_configs);
	resman_configs = NULL;
	list_for_each_safe(pos, tmp, &resources_head) {
		resource = list_entry(pos, struct resman_resource, list);

		list_for_each_safe(pos1, tmp1, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			resman_release_resource(node);
		}
		resman_destroy_resource(resource);
	}
	mutex_unlock(&resource_lock);
}

#ifdef RESMAM_ENABLE_JSON
/**
 * resman_config_from_json() - parse resource config
 *
 * @buf: config string
 *
 * Parse config
 * The config is a json file that contains various of resource config arrays
 * Every resource config contains resource name and type and optional args
 *
 * config_string
 *     {   "resman_config": [
 *         {
 *          "name": "resname1",
 *          "type": type1,
 *          "args": "args1"
 *         },
 *         {
 *          "name": "resname2",
 *          "type": type1,
 *          "args": "args1"
 *         }
 *      ]
 *     }
 *
 * Return: true for success or false
 */

static bool resman_config_from_json(char *buf)
{
	bool ret = true;
	char *configs = kstrdup(buf, GFP_KERNEL);
	char *newconfig = NULL;
	struct cjson *root = NULL, *arrs_node = NULL;
	struct cjson *node = NULL, *name_node = NULL, *type_node = NULL, *args_node = NULL;

	if (!configs | !buf) {
		dprintk(0, "read %s\n", buf);
		return false;
	}

	if (configs[strlen(configs) - 1] == '\n')
		configs[strlen(configs) - 1] = 0;

	root = cjson_parse(configs);
	if (root == 0) {
		dprintk(0, "json null or has error, please check the json file:[ %s]\n", configs);
		ret = false;
		goto error;
	}
	arrs_node = cjson_getobjectitem(root, "resman_config");
	if (arrs_node == 0) {
		ret = false;
		goto error;
	}
	mutex_lock(&resource_lock);

	if (arrs_node->type == cjson_array) {
		node = arrs_node->child;
		while (node) {
			name_node = cjson_getobjectitem(node, "name");
			if (!name_node) {
				ret = false;
				mutex_unlock(&resource_lock);
				goto error;
			}
			dprintk(3, "%s:%s\n", name_node->string, name_node->valuestring);
			type_node = cjson_getobjectitem(node, "type");
			if (!type_node) {
				ret = false;
				mutex_unlock(&resource_lock);
				goto error;
			}
			dprintk(3, "type:%d\n", type_node->valueint);
			args_node = cjson_getobjectitem(node, "args");
			if (args_node)
				dprintk(3, "args:%s\n", args_node->valuestring);
			else
				dprintk(3, "args = NULL");
			if (!resman_create_resource(name_node->valuestring, type_node->valueint,
					args_node ? args_node->valuestring : NULL)) {
				ret = false;
				break;
			}
			node = node->next;
		}
	}

	mutex_unlock(&resource_lock);
	if (!ret) {
		dprintk(0, "parse config failed\n%s\n", buf);
		all_resource_uninit();
	} else if (resman_configs) {
		newconfig = kzalloc(strlen(resman_configs) + strlen(buf) + 1,
					GFP_KERNEL);
		if (newconfig) {
			memcpy(newconfig, resman_configs,
				strlen(resman_configs));
			memcpy(newconfig + strlen(resman_configs),
				buf, strlen(buf));
			kfree(resman_configs);
			resman_configs = newconfig;
		}
	} else {
		resman_configs = kstrdup(buf, GFP_KERNEL);
	}

error:
	cjson_delete(root);
	kfree(configs);
	return ret;
}
#else
/**
 * resman_parser_config() - parse resource config
 *
 * @buf: config string
 *
 * Parse config
 * The config is a string that contains various of resource config split by ';'
 * Every resource config contains resource name and key-value pairs split by ','
 *
 * config_string
 *     resource_config[];
 *         {
 *             resource_name,
 *             type: type_value,
 *             [key0: int_value0, key1:int_value1, ...]
 *         }
 * eg: "resource0,type:1;resource1,type:1,key0:1,key1:2;"
 *
 * Return: true for success or false
 */

static bool resman_parser_config(const char *buf)
{
	bool ret = true;
	char *config, *cur;
	char *name, *type_str, *arg;
	__u32 type;
	char *configs = kstrdup(buf, GFP_KERNEL);
	char *newconfig = NULL;

	if (!configs | !buf)
		return false;

	cur = configs;
	if (configs[strlen(configs) - 1] == '\n')
		configs[strlen(configs) - 1] = 0;

	mutex_lock(&resource_lock);
	while ((config = strsep(&cur, ";"))) {
		name = strsep(&config, ",");
		if (!name) {
			ret = false;
			break;
		}
		if (!strlen(name))
			break;
		type_str = strsep(&config, ",");
		if (!type_str) {
			ret = false;
			break;
		}
		;
		if (!resman_parser_kv(type_str, "type", &type)) {
			ret = false;
			break;
		}
		arg = config;
		if (!resman_create_resource(name, type, arg)) {
			ret = false;
			break;
		}
	}

	mutex_unlock(&resource_lock);
	if (!ret) {
		dprintk(0, "parse config failed\n%s\n", buf);
		all_resource_uninit();
	} else if (resman_configs) {
		newconfig = kzalloc(strlen(resman_configs) + strlen(buf) + 1,
					GFP_KERNEL);
		if (newconfig) {
			memcpy(newconfig, resman_configs,
				strlen(resman_configs));
			memcpy(newconfig + strlen(resman_configs),
				buf, strlen(buf));
			kfree(resman_configs);
			resman_configs = newconfig;
		}
	} else {
		resman_configs = kstrdup(buf, GFP_KERNEL);
	}
	kfree(configs);
	return ret;
}

#endif


static void all_resource_init(void)
{
#ifdef RESMAM_ENABLE_JSON
/*
 * default resman_config json
 * {
 *	   "resman_config": [
 *		   {
 *			   "name": "vfm_default",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "amvideo",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "videopip",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "tvp",
 *			   "type": 3
 *		   },
 *		   {
 *			   "name": "tsparser",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "codec_mm",
 *			   "type": 4,
 *			   "args": "total:0"
 *		   },
 *		   {
 *			   "name": "adc_pll",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "decoder",
 *			   "type": 1,
 *			   "args": "avail:9"
 *		   }
 *	   ]
 * }
 */
	char *default_configs = NULL;
	int len = 0, i = 0;
	char c[10][100] = {
	"{\"resman_config\":[{\"name\":\"vfm_default\",\"type\":1,\"args\":\"avail:1\"},",
	"{\"name\": \"amvideo\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"videopip\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"tvp\",\"type\": 3},",
	"{\"name\": \"tsparser\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"codec_mm\",\"type\": 4,\"args\": \"total:0\"},",
	"{\"name\": \"adc_pll\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"decoder\",\"type\": 1,\"args\": \"avail:9\"},",
	"{\"name\": \"videoplane\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"audiodecoder\",\"type\": 1,\"args\": \"avail:2\"}]}"};

	len = strlen(c[0]) + strlen(c[1]) + strlen(c[2]) + strlen(c[3]) +
			strlen(c[4]) + strlen(c[5]) + strlen(c[6]) + strlen(c[7]) +
			strlen(c[8]) + strlen(c[9]);
	default_configs = kzalloc(len + 1, GFP_KERNEL);
	len = 0;
	if (default_configs) {
		for (i = 0; i < 10; i++) {
			memcpy(default_configs + len, c[i], strlen(c[i]));
			len +=  strlen(c[i]);
		}
	}
#else
	char *default_configs =
		"vfm_default,type:1,avail:1;"
		"amvideo,type:1,avail:1;"
		"videopip,type:1,avail:1;"
		"tvp,type:3;"
		"tsparser,type:1,avail:1;"
		"codec_mm,type:4,total:0;"
		"adc_pll,type:1,avail:1;"
		"decoder,type:1,avail:9;"
		"dmx,type:1,avail:4;"
		"di,type:1,avail:4;"
		"hwc,type:1,avail:9;";
#endif
	INIT_LIST_HEAD(&sessions_head);
	INIT_LIST_HEAD(&resources_head);
	INIT_LIST_HEAD(&debug_head);
	INIT_LIST_HEAD(&error_info_head);
#ifdef RESMAM_ENABLE_JSON
	if (default_configs && len && default_configs[len] == '\0')
		resman_config_from_json(default_configs);
	kfree(default_configs);
#else
	resman_parser_config(default_configs);
#endif
}

static long resman_ioctl_acquire(struct resman_session *sess,
				 unsigned long para)
{
	long r = 0;
	struct resman_resource *resource;
	struct resman_para __user *argp = (void __user *)para;
	struct resman_para resman;
	int select_res;
	bool preempt = false;
	__u32 timeout = 0;
	char *arg = NULL;

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		/*Compat with old userspace lib*/
		select_res = (int)para;
	} else {
		select_res = resman.k;
		preempt = !!resman.v.acquire.preempt;
		timeout = resman.v.acquire.timeout;
		arg = resman.v.acquire.arg;
	}

	resource = resman_find_resource_by_id(select_res);
	if (!resource)
		return -EINVAL;
	mutex_lock(&sess->lock);
	if (resman_find_node_by_resource_id(sess, select_res)) {
		r = 0;
		goto beach;
	}

	if (!resman_acquire_resource(sess, resource, preempt, timeout, arg))
		r = -EBUSY;

beach:
	mutex_unlock(&sess->lock);
	return r;
}

static int resman_estimate_tvp_available(int size)
{
	int free_size = codec_mm_get_free_size();
	int tvp_fhd = 0, tvp_uhd = 0, mode = 0;

	free_size += codec_mm_get_tvp_free_size();
	if (size <= 0) {
		codec_mm_get_default_tvp_size(&tvp_fhd, &tvp_uhd);
		if (free_size >= tvp_uhd)
			mode = 2;
		else if (free_size > tvp_fhd)
			mode = 1;
	} else {
		if (free_size >= size)
			mode = 2;
	}
	return mode;
}

static int resman_codec_mm_available(struct resman_session *sess, int ifsecure)
{
	int avail = 0;

	dprintk(2, "%d ifsecure %d\n", sess->id, ifsecure);
	if (ifsecure == QUERY_NO_SECURE_BUFFER)
		avail = codec_mm_get_free_size() >> 20;
	if (ifsecure == QUERY_SECURE_BUFFER) {
		avail = (codec_mm_get_free_size() +
			codec_mm_get_tvp_free_size()) >> 20;
	}
	dprintk(2, "%d avail %d\n", sess->id, avail);
	if (resman_debug > 1)
		dump_mem_infos_external();

	return avail;
}

static long resman_ioctl_query(struct resman_session *sess, unsigned long para)
{
	long r = 0;
	int select_res = 0;
	struct resman_para __user *argp = (void __user *)para;
	char usage[32];
	struct resman_para resman;
	struct resman_resource *resource;

	memset(usage, 0, sizeof(usage));

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		return -EINVAL;
	}
	select_res = resman.k;
	resource = resman_find_resource_by_id(select_res);
	if (resource) {
		memcpy(resman.v.query.name,
			resource->name, sizeof(resman.v.query.name));
		resman.v.query.type = resource->type;

	switch (resource->type) {
	case RESMAN_TYPE_COUNTER:
		resman.v.query.value = resource->value;
		resman.v.query.avail =
			resource->d.counter.avail;
		if (resman.v.query.value > 0) {
			memset(resman.v.query.app_name, 0,
				sizeof(resman.v.query.app_name));
			resman_find_appname_by_resource_name(resman.v.query.name,
				resman.v.query.app_name);
		}
		break;
	case RESMAN_TYPE_CODEC_MM:
		resman.v.query.avail =
			resman_codec_mm_available(sess, resman.v.query.value);
		resman.v.query.value = codec_mm_get_total_size() >> 20;
		break;
	case RESMAN_TYPE_CAPACITY_SIZE:
		resman.v.query.value = resource->value;
		resman.v.query.avail =
			(resource->d.capacity.total > resource->value) ?
			(resource->d.capacity.total - resource->value) : 0;
		break;
	case RESMAN_TYPE_TVP:
		resman.v.query.avail =
			resman_estimate_tvp_available(resman.v.query.value);
		break;
	default:
		break;
	}

	if (copy_to_user((void *)argp,
			&resman, sizeof(resman)))
		r = -EFAULT;
	} else {
		r = -EINVAL;
	}
	return r;
}

static long resman_ioctl_release(struct resman_session *sess,
				 unsigned long para)
{
	long r = 0;
	int select_res = (int)para;
	struct resman_resource *resource;
	struct resman_node  *node;
	dprintk(2, "%d appname:%s release, type=%d\n",
			sess->id,
			sess->app_name,
			sess->app_type);
	resource = resman_find_resource_by_id(select_res);
	if (!resource)
		return -EINVAL;

	mutex_lock(&sess->lock);
	node = resman_find_node_by_resource_id(sess, select_res);
	if (node)
		resman_release_resource(node);
	mutex_unlock(&sess->lock);
	return r;
}

static long resman_ioctl_setappinfo(struct resman_session *sess,
				    unsigned long para)
{
	long r = 0;
	struct app_info __user *argp = (void __user *)para;
	struct app_info appinfo;

	if (copy_from_user((void *)&appinfo, argp, sizeof(struct app_info)) ||
		!sess) {
		r = -EINVAL;
	} else {
		strncpy(sess->app_name, appinfo.app_name,
			sizeof(sess->app_name));
		sess->app_name[sizeof(sess->app_name) - 1] = '\0';
		sess->app_type = appinfo.app_type;
		sess->prio = appinfo.prio;
		dprintk(2, "%d appname:%s, type = %d prio = %d\n",
			sess->id,
			sess->app_name,
			sess->app_type,
			sess->prio);
	}
	return r;
}

static long resman_ioctl_check_support(struct resman_session *sess,
					 unsigned long para)
{
	long r = 0;
	struct resman_para __user *argp = (void __user *)para;
	struct resman_para resman;

	memset(&resman, 0, sizeof(resman));

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		r = -EINVAL;
	} else {
		if (resman.k > sizeof(resman.v.support.name) - 1)
			return -EINVAL;

		if (resman_find_resource_by_name(resman.v.support.name))
			return 0;
		r = -EINVAL;
	}

	return r;
}

static long resman_ioctl_load_res(struct resman_session *sess,
				 unsigned long para)
{
	long r = 0;
	struct res_item __user *argp = (void __user *)para;
	struct res_item item;
	int arglen = 0;
	char *itemarg = NULL;

	mutex_lock(&resource_lock);
	item.name[sizeof(item.name) - 1] = '\0';
	item.arg[sizeof(item.arg) - 1] = '\0';
	if (copy_from_user((void *)&item, argp, sizeof(item))) {
		dprintk(1, "%d load res FAIL!!\n", sess->id);
	} else {
		item.name[sizeof(item.name) - 1] = '\0';
		item.arg[sizeof(item.arg) - 1] = '\0';
		if (strlen(item.name) > 0 && strlen(item.name) < RESMAN_STR_MAX_SIZE) {
			if (strlen(item.arg) > 0 && strlen(item.arg) < RESMAN_STR_MAX_SIZE)
				arglen = strlen(item.arg);
			if (arglen > 0 && arglen < RESMAN_STR_MAX_SIZE)
				itemarg = item.arg;
			if (!resman_create_resource(item.name, item.type, itemarg))
				r = -1;
		}
	}
	mutex_unlock(&resource_lock);
	return r;
}

static long resman_ioctl_release_all(struct resman_session *sess,
				     unsigned long para)
{
	long r = 0;
	struct resman_node  *node;
	struct list_head *pos, *tmp;

	mutex_lock(&sess->lock);
	list_for_each_safe(pos, tmp, &sess->resources) {
		node = list_entry(pos, struct resman_node, rlist);
		resman_release_resource(node);
	}
	mutex_unlock(&sess->lock);
	return r;
}

static ssize_t resman_usage_show(const struct class *class,
			  const struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;

	resman_usage_dump();
	return size;
}

static ssize_t resman_config_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t size = 0;
	if (resman_configs)
		APPEND_ATTR_BUF("%s\n", resman_configs)
	return size;
}

static ssize_t resman_config_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	all_resource_uninit();
#ifdef RESMAM_ENABLE_JSON
	resman_config_from_json((char *)buf);
#else
	resman_parser_config(buf);
#endif
	return size;
}

static ssize_t resman_ver_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t size = 0;

	APPEND_ATTR_BUF("V%d.%d.%d\n", RESMAN_VERSION, RESMAN_SUB_VERSION, RESMAN_SUBMIT_NUMBER)
	return RESMAN_VERSION;
}

static ssize_t resman_res_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t size = 0;
	struct list_head *pos1, *tmp1;
	struct resman_resource *resource;

	mutex_lock(&resource_lock);
	APPEND_ATTR_BUF("%-32s", "name")
	APPEND_ATTR_BUF("%-10s\n", "avail")
	list_for_each_safe(pos1, tmp1, &resources_head) {
		resource = list_entry(pos1, struct resman_resource, list);
		APPEND_ATTR_BUF("%-32s", resource->name)
		if (resource->type == RESMAN_TYPE_COUNTER)
			APPEND_ATTR_BUF("%d", resource->d.counter.avail)
		else if (resource->type == RESMAN_TYPE_CODEC_MM)
			APPEND_ATTR_BUF("total%d uhd%d fhd%d secure%d, counts%d countnon%d",
				resource->d.codec_mm.total, resource->d.codec_mm.uhd,
				resource->d.codec_mm.fhd, resource->d.codec_mm.secure,
				resource->d.codec_mm.counter_secure.counter,
				resource->d.codec_mm.counter_nonsecure.counter)
		else if (resource->type == RESMAN_TYPE_CAPACITY_SIZE)
			APPEND_ATTR_BUF("%d", resource->d.capacity.total)
		APPEND_ATTR_BUF("%s", "\n")
	}
	mutex_unlock(&resource_lock);
	return size;
}

static ssize_t res_report_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	struct resman_session *sess = NULL;
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;

	mutex_lock(&sessions_lock);
	list_for_each_safe(pos, tmp, &sessions_head) {
		sess = list_entry(pos, struct resman_session, list);
		if (sess) {
			resman_send_event(sess, RESMAN_EVENT_RESREPORT);
			wake_up_interruptible(&sess->wq_event);
		}
	}
	mutex_unlock(&sessions_lock);
	return 0;
}

static ssize_t res_sys_debug_show(const struct class *class,
			   const struct class_attribute *attr,
			   char *buf)
{
	ssize_t size = 0;

	if (debug_info)
		size = strlen(debug_info);

	APPEND_ATTR_BUF("%s\n", debug_info)

	return size;
}

static ssize_t res_sys_debug_store(const struct class *class,
			    const struct class_attribute *attr,
			    const char *buf, size_t size)
{
	struct module_debug_node *node = NULL;
	struct resman_session *sess = NULL;
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;
	int debug_len = 0;

	mutex_lock(&debug_lock);
	if (buf && size > 0) {
		if (!debug_info) {
			debug_info = kzalloc(PAGE_SIZE, GFP_KERNEL);
			if (!debug_info) {
				dprintk(0, "failed allocate debug info\n");
				mutex_unlock(&debug_lock);
				return -ENOMEM;
			}
		}

		memset(debug_info, 0, PAGE_SIZE);
		debug_len = size > PAGE_SIZE ? PAGE_SIZE : size;
		memcpy(debug_info, buf, debug_len);
	}

	list_for_each_safe(pos, tmp, &debug_head) {
		node = list_entry(pos, struct module_debug_node, list);
		if (node->callback)
			node->callback(node->module, debug_info, debug_len);
	}

	mutex_unlock(&debug_lock);

	mutex_lock(&sessions_lock);
	list_for_each_safe(pos, tmp, &sessions_head) {
		sess = list_entry(pos, struct resman_session, list);
		if (sess) {
			resman_send_event(sess, RESMAN_EVENT_DEBUGEVENT);
			wake_up_interruptible(&sess->wq_event);
		}
	}
	mutex_unlock(&sessions_lock);

	return size;
}

#undef APPEND_ATTR_BUF

int resman_register_query_ops_by_name(const char *device_name,
			int (*fn)(struct resman_cb_param_any_t q))
{
	struct resman_query_cb_ops *ops;
	size_t len;
	int ret = 0;
	char *null_pos = NULL;

	if (!device_name || !fn)
		return -EINVAL;
	null_pos = memchr(device_name, '\0', RESMAN_LEN);
	len = (null_pos) ? (null_pos - device_name) : RESMAN_LEN;
	if (len == 0 || len >= RESMAN_LEN)
		return -EINVAL;

	mutex_lock(&resman_cb_ops_mutex);

	list_for_each_entry(ops, &g_cb_ops_list, list) {
		if (strncmp(ops->device_name, device_name, RESMAN_LEN) == 0) {
			ops->query_resource_cb = fn;
			dprintk(0, "resman: updated cb for device %s\n", device_name);
			goto out_unlock;
		}
	}

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (!ops) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	strscpy(ops->device_name, device_name, sizeof(ops->device_name));
	ops->query_resource_cb = fn;
	INIT_LIST_HEAD(&ops->list);
	list_add_tail(&ops->list, &g_cb_ops_list);
	dprintk(0, "resman: registered cb for device %s\n", device_name);

out_unlock:
	mutex_unlock(&resman_cb_ops_mutex);
	return ret;
}
EXPORT_SYMBOL(resman_register_query_ops_by_name);

static int resman_session_query_resource_by_name(struct resman_session *sess,
		const char *device_name,
		struct  resman_cb_param_any_t q)
{
	struct resman_query_cb_ops *ops;
	int ret = -ENOENT;
	size_t len;
	char *null_pos = NULL;

	if (!sess || !device_name)
		return -EINVAL;
	null_pos = memchr(device_name, '\0', RESMAN_LEN);
	len = (null_pos) ? (null_pos - device_name) : RESMAN_LEN;
	if (len == 0 || len >= RESMAN_LEN)
		return -EINVAL;

	mutex_lock(&resman_cb_ops_mutex);
	if (!resman_is_valid_device_name(device_name)) {
		dprintk(0, "resman: invalid device name %s\n", device_name);
		ret = -EINVAL;
		goto out_unlock;
	}

	list_for_each_entry(ops, &g_cb_ops_list, list) {
		if (strncmp(ops->device_name, device_name, RESMAN_LEN) == 0) {
			if (ops->query_resource_cb)
				ret = ops->query_resource_cb(q);
			goto out_unlock;
		}
	}
	ret = resman_register_default_query_ops(q);
	dprintk(0, "resman: callback not found for module %s, used default\n", device_name);

out_unlock:
	mutex_unlock(&resman_cb_ops_mutex);
	return ret;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
static int resman_open(struct inode *inode, struct file *filp)
{
	struct resman_session *sess;

	sess = resman_open_session();
	if (!sess)
		return -ENOMEM;

	filp->private_data = sess;
	dprintk(2, "%d open\n",
		sess->id);
	return 0;
}

static ssize_t resman_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	ssize_t ret = -EFAULT;
	struct resman_session *sess;
	struct resman_event *event;

	sess = filp->private_data;
	if (len < sizeof(__u32))
		return -EINVAL;
	mutex_lock(&sess->lock);
	event = list_first_entry_or_null(&sess->events,
					 struct resman_event, list);
	if (event) {
		if (!copy_to_user(buf, &event->type, sizeof(event->type))) {
			ret = sizeof(event->type);
			list_del(&event->list);
			kfree(event);
		}
	}
	mutex_unlock(&sess->lock);
	return ret;
}

static unsigned int resman_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct resman_session *sess;

	sess = filp->private_data;
	poll_wait(filp, &sess->wq_event, wait);
	if (!list_empty(&sess->events))
		mask |= POLL_IN | POLLRDNORM;

	return mask;
}

static long resman_ioctl_get_sys_debug_level(struct resman_session *sess, unsigned long para)
{
	int len = 0;

	if (debug_info)
		len = strlen(debug_info);

	if (len > 0) {
		if (len > PAGE_SIZE)
			len = PAGE_SIZE;

		if (copy_to_user((void __user *)para, debug_info, len)) {
			len = 0;
			dprintk(0, "error copy to use space");
		}
	}

	return len;
}

static long resman_ioctl_get_counter_index(struct resman_session *sess, unsigned long para)
{
	long r = -1;
	int select_res = 0, i = 0;
	__u32 id = 0;
	struct resman_resource *resource;

	if (copy_from_user((void *)&id, (void __user *)para, sizeof(id))) {
		dprintk(0, "getcount index para error\n");
		return -EINVAL;
	}

	select_res = id;
	resource = resman_find_resource_by_id(select_res);
	if (!resource)
		return r;
	for (i = 0; i < resource->d.counter.counter_list->size; i++) {
		if (resource->d.counter.counter_list->items[i].state == 1 &&
			resource->d.counter.counter_list->items[i].ssid == sess->id) {
			r = i;
			break;
		}
	}
	return r;
}

static long resman_ioctl_get_version(struct resman_session *sess, unsigned long para)
{
	struct resman_version __user *pver = (void __user *)para;
	struct resman_version ver;

	if (!pver)
		return -EINVAL;
	memset(&ver, 0, sizeof(ver));
	ver.ver = RESMAN_VERSION;
	ver.sub_ver = RESMAN_SUB_VERSION;
	ver.submit_number = 0;

	if (copy_to_user((void *)pver, &ver, sizeof(ver)))
		return -EFAULT;

	return 0;
}

static int resman_ioctl_parameter_set_ctl(struct resman_session *sess, unsigned long para)
{
	int r = -1, r_bytes = 0;

	struct resman_param_set_control resman_set_ctl;
	struct resman_param_item *resman_set_item_t = NULL;
	struct resman_cb_param_any_t resman_cb_q;

	int w = 0, h = 0, dw = 0, format = 0, margin = 0, bitdepth = 0, v_status = 0;
	int is_interlace = 0, dpb = 0, total = 0, pipeline = 1, secure = 0;

	if (copy_from_user(&resman_set_ctl,
		 (void __user *)para,
		sizeof(resman_set_ctl))) {
		return -EFAULT;
	}
	//dprintk(4, "resman_set_ctl.cmd = %d\n", resman_set_ctl.cmd);

	switch (resman_set_ctl.cmd) {
	case SET_INT32_INFO:
	{
		mutex_lock(&sess->lock);
		r = param_set_update_int(sess->param_set, resman_set_ctl.name, resman_set_ctl.value,
			resman_set_ctl.desc, resman_set_ctl.module_name, resman_set_ctl.domain,
			resman_set_ctl.sub_domain, resman_set_ctl.instance_id,
			resman_set_ctl.process_id, resman_set_ctl.thread_id);
		mutex_unlock(&sess->lock);
		break;
	}
	case SET_INT64_INFO:
	{
		mutex_lock(&sess->lock);
		r = param_set_update_int64(sess->param_set, resman_set_ctl.name,
			resman_set_ctl.value64, resman_set_ctl.desc, resman_set_ctl.module_name,
			resman_set_ctl.domain, resman_set_ctl.sub_domain,
			resman_set_ctl.instance_id, resman_set_ctl.process_id,
			resman_set_ctl.thread_id);
		mutex_unlock(&sess->lock);
		break;
	}
	case SET_STRING_INFO:
	{
		mutex_lock(&sess->lock);
		r = param_set_update_string(sess->param_set, resman_set_ctl.name,
			(char __user *)(uintptr_t)resman_set_ctl.ptr, resman_set_ctl.desc,
			resman_set_ctl.module_name, resman_set_ctl.domain,
			resman_set_ctl.sub_domain, resman_set_ctl.instance_id,
			resman_set_ctl.process_id, resman_set_ctl.thread_id);
		mutex_unlock(&sess->lock);
		break;
	}
	case SET_CUSTOM_INFO:
	{
		mutex_lock(&sess->lock);
		r = param_set_update_custom_set(sess->param_set, resman_set_ctl.name,
			(struct resman_param_set __user *)(uintptr_t)resman_set_ctl.ptr,
			resman_set_ctl.desc, resman_set_ctl.module_name, resman_set_ctl.domain,
			resman_set_ctl.sub_domain, resman_set_ctl.instance_id,
			resman_set_ctl.process_id, resman_set_ctl.thread_id);
		mutex_unlock(&sess->lock);
		break;
	}
	case GET_INT32_INFO:
		if (resman_set_item_t)
			r = resman_set_item_t->value.i;
		//dprintk(0, "resman_set_item_t.value.i= %d\n", resman_set_item_t->value.i);
		break;
	case GET_INT64_INFO:
		if (resman_set_item_t)
			r = resman_set_item_t->value.i64;
		//dprintk(0, "resman_set_item_t.value.i= %d\n", resman_set_item_t->value.i64);
		break;
	case GET_CODEC_MM_SIZE_MIN:
		mutex_lock(&sess->lock);
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_W));
		w = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_H));
		h = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_DW));
		dw = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_FMT));
		format = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_MARGIN));
		margin = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_FD));
		is_interlace = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_SECURE));
		secure = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_PIPELINE));
		pipeline = !resman_set_item_t ? 1 : resman_set_item_t->value.i;
		if (secure) {
			resman_set_item_t = param_set_get_by_name(sess->param_set,
					get_resman_param_key_string(RESMAN_PARAM_VIDEO_BITDEPTH));
			bitdepth = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		}

		dprintk(4, "%d w=%d h=%d dw=%d fmt=%d margin=%d di=%d pipe=%d secure=%d bit=%d\n",
					sess->id, w, h, dw, format, margin, is_interlace,
					pipeline, secure, bitdepth);
		memset(&resman_cb_q, 0, sizeof(resman_cb_q));
		resman_cb_q.size = sizeof(resman_cb_q);
		resman_cb_q.type = RESMAN_CB_PARAM_EST;
		resman_cb_q.sub_type = RESMAN_MEM_USAGE_MIN;
		resman_cb_q.v.est.video_format = format;
		resman_cb_q.v.est.width = w;
		resman_cb_q.v.est.height = h;
		resman_cb_q.v.est.dw = dw;
		resman_cb_q.v.est.margin_num = margin;
		resman_cb_q.v.est.is_tvp = secure;
		resman_cb_q.v.est.is_android = true;
		resman_cb_q.v.est.is_interlace = is_interlace;
		resman_cb_q.v.est.pipeline = pipeline;
		resman_cb_q.v.est.bitdepth = bitdepth;
		r_bytes = resman_session_query_resource_by_name(sess,
						  RESMAN_DEV_VDEC,
						  resman_cb_q);
		if (r_bytes > RESMAN_BYTES_MAX) {
			dprintk(0, "%d bytes too large: %d\n", sess->id, r_bytes);
			r_bytes = RESMAN_BYTES_MAX;
		}
		if (r_bytes >= 0)
			r = r_bytes / (1024 * 1024);
		if (r <= 0)
			r = RESMAN_DEFAULIT_MIN_SIZE_FOR_VDEC;
		dprintk(4, "%d min size %d\n", sess->id, r);
		mutex_unlock(&sess->lock);
		break;
	case GET_CODEC_MM_SIZE_EXPECTED:
		mutex_lock(&sess->lock);
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_W));
		w = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_H));
		h = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_DW));
		dw = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_FMT));
		format = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_MARGIN));
		margin = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_FD));
		is_interlace = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_DPB));
		dpb = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_SECURE));
		secure = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_PIPELINE));
		pipeline = !resman_set_item_t ? 1 : resman_set_item_t->value.i;
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_TOTALNUM));
		total = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		if (secure) {
			resman_set_item_t = param_set_get_by_name(sess->param_set,
					get_resman_param_key_string(RESMAN_PARAM_VIDEO_BITDEPTH));
			bitdepth = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		}

		dprintk(4, "%d w=%d h=%d dw=%d fmt=%d margin=%d dpb=%d di=%d bit=%d yuv=%d\n",
			sess->id, w, h, dw, format, margin, dpb, is_interlace, bitdepth, total);
		if (total > (margin + dpb)) {
			margin = total - dpb;
			dprintk(5, "%d adjust margin %d dpb %d total %d\n", sess->id,
				margin, dpb, total);
		}
		memset(&resman_cb_q, 0, sizeof(resman_cb_q));
		resman_cb_q.size = sizeof(resman_cb_q);
		resman_cb_q.type = RESMAN_CB_PARAM_EST;
		resman_cb_q.sub_type = RESMAN_MEM_USAGE_EXPECTED;
		resman_cb_q.v.est.video_format = format;
		resman_cb_q.v.est.width = w;
		resman_cb_q.v.est.height = h;
		resman_cb_q.v.est.dw = dw;
		resman_cb_q.v.est.margin_num = margin;
		resman_cb_q.v.est.dpb_num = dpb;
		resman_cb_q.v.est.is_tvp = secure;
		resman_cb_q.v.est.is_android = true;
		resman_cb_q.v.est.is_interlace = is_interlace;
		resman_cb_q.v.est.pipeline = pipeline;
		resman_cb_q.v.est.bitdepth = bitdepth;
		r = 0;
		//vdec buffer
		if (pipeline != RESMAN_PLAYBACK_PIPELINE_HDMI_DEFAULT) {
			r_bytes = resman_session_query_resource_by_name(sess,
						RESMAN_DEV_VDEC, resman_cb_q);
			if (r_bytes > RESMAN_BYTES_MAX) {
				dprintk(0, "%d bytes too large: %d\n", sess->id, r_bytes);
				r_bytes = RESMAN_BYTES_MAX;
			}
			if (r_bytes >= 0)
				r = r_bytes / (1024 * 1024);
		}
		dprintk(4, "%d vdec expect size %d\n", sess->id, r);
#if defined(CONFIG_AMLOGIC_DPSS_PROCESS) || defined(CONFIG_AMLOGIC_DI_PROCESS)
		// di buffer
		r_bytes = get_di_backend_need_mem(w, h, is_interlace);
		if (r_bytes > 0 && r_bytes < RESMAN_BYTES_MAX) {
			if (r > INT_MAX - r_bytes)
				r = INT_MAX;
			else
				r += r_bytes;
		}
#endif
		//demux buffer
		if (!secure && (pipeline == RESMAN_PLAYBACK_PIPELINE_V4L2_STREAMMODE ||
			pipeline == RESMAN_PLAYBACK_PIPELINE_AMPORT_STREAMMODE))
			r += RESMAN_DEMUX_VIDEO_SIZE + RESMAN_DEMUX_AUDIO_SIZE;
		dprintk(4, "%d vdec expect size new %d\n", sess->id, r);
		mutex_unlock(&sess->lock);
		break;
	case GET_CODEC_MM_SIZE_ACTUAL:
		mutex_lock(&sess->lock);
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_SECURE));
		secure = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		memset(&resman_cb_q, 0, sizeof(resman_cb_q));
		resman_cb_q.size = sizeof(resman_cb_q);
		resman_cb_q.type = RESMAN_CB_PARAM_DEC_STATUS;
		resman_cb_q.v.dst.ssid = sess->id;
		resman_cb_q.v.dst.flag = secure;
		r = 0;
		//vdec buffer
		if (pipeline != RESMAN_PLAYBACK_PIPELINE_HDMI_DEFAULT) {
			r_bytes = resman_session_query_resource_by_name(sess,
						RESMAN_DEV_VDEC, resman_cb_q);
			if (r_bytes > RESMAN_BYTES_MAX) {
				dprintk(0, "%d bytes too large: %d\n", sess->id, r_bytes);
				r_bytes = RESMAN_BYTES_MAX;
			}
			if (r_bytes >= 0)
				r = r_bytes / (1024 * 1024);
		}
		dprintk(4, "%d realtime size %d\n", sess->id, r);
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_PIPELINE));
		pipeline = !resman_set_item_t ? 1 : resman_set_item_t->value.i;
		//demux buffer
		if (!secure && (pipeline == RESMAN_PLAYBACK_PIPELINE_V4L2_STREAMMODE ||
			pipeline == RESMAN_PLAYBACK_PIPELINE_AMPORT_STREAMMODE))
			r += RESMAN_DEMUX_VIDEO_SIZE + RESMAN_DEMUX_AUDIO_SIZE;
		dprintk(4, "%d realtime size new size %d\n", sess->id, r);
		//di buffer
		resman_set_item_t = param_set_get_by_name(sess->param_set,
			get_resman_param_key_string(RESMAN_PARAM_VIDEO_STATUS));
		v_status = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
		if (v_status > RESMAN_STATUS_PREPARING) {
#if defined(CONFIG_AMLOGIC_DPSS_PROCESS) || defined(CONFIG_AMLOGIC_DI_PROCESS)
			resman_set_item_t = param_set_get_by_name(sess->param_set,
					get_resman_param_key_string(RESMAN_PARAM_VIDEO_FD));
			is_interlace = !resman_set_item_t ? 0 : resman_set_item_t->value.i;
			r_bytes = get_di_backend_need_mem(w, h, is_interlace);
			if (r_bytes > 0 && r_bytes < RESMAN_BYTES_MAX) {
				if (r > INT_MAX - r_bytes)
					r = INT_MAX;
				else
					r += r_bytes;
			}
#endif
			dprintk(4, "%d realtime size new %d\n", sess->id, r);
		}
		mutex_unlock(&sess->lock);
		break;
	case GET_INSTANCE_SSID:
		r = sess->id;
		break;
	case GET_ITEM_BY_NAME:
		//dprintk(0, "GET_ITEM_BY_NAME resman_set_ctl.name = %s\n", resman_set_ctl.name);
		resman_set_item_t = param_set_get_by_name(sess->param_set, resman_set_ctl.name);
		if (!resman_set_item_t)
			break;
		//dprintk(0, "GET_ITEM_BY_NAME = %d\n", resman_set_item_t->value.i);
		if (copy_to_user((void __user *)(uintptr_t)resman_set_ctl.ptr,
			(void *)resman_set_item_t,  sizeof(*resman_set_item_t))) {
			dprintk(0, "GET_ITEM_BY_NAME copy_to_user fail 1\n");
			break;
		}
		if (copy_to_user((void __user *)para, &resman_set_ctl, sizeof(resman_set_ctl))) {
			dprintk(0, "GET_ITEM_BY_NAME copy_to_user fail 2\n");
			break;
		}
		break;
	case UPDATE_CODEC_MM_SIZE_ACQUIRED:
		r = resman_codec_mm_update_size(sess, resman_set_ctl.value);
		break;
	default:
		break;
	}
	return r;
}

static long resman_ioctl_set_sys_debug_level(struct resman_session *sess, unsigned long para)
{
	long r = 0;
	ssize_t size = 0;
	struct debug_level __user *argp = (void __user *)para;
	struct debug_level *debuglevel_ptr = NULL;

	if (!argp)  {
		r = -EFAULT;
		goto error;
	}

	debuglevel_ptr = kzalloc(sizeof(*debuglevel_ptr), GFP_KERNEL);
	if (!debuglevel_ptr) {
		r = -ENOMEM;
		goto error;
	}

	if (copy_from_user((void *)debuglevel_ptr, argp, sizeof(*debuglevel_ptr))) {
		r = -EFAULT;
		goto error;
	}

	if (debuglevel_ptr->len == 0 || debuglevel_ptr->len > PAGE_SIZE ||
		debuglevel_ptr->debug_info[0] == '\0') {
		r = -EINVAL;
		goto error;
	}

	dprintk(3, "info =%s\n", debuglevel_ptr->debug_info);
	size = res_sys_debug_store(NULL, NULL, debuglevel_ptr->debug_info,
		debuglevel_ptr->len);
	if (size <= 0) {
		r = -EINVAL;
		goto error;
	}
	r = 0;
error:
	kfree(debuglevel_ptr);
	return r;
}

static long resman_ioctl_get_error_info(struct resman_session *sess, unsigned long para)
{
	int len = 0;
	struct error_info_node *node = NULL;
	struct list_head *pos = NULL, *tmp = NULL;

	list_for_each_safe(pos, tmp, &error_info_head) {
		node = list_entry(pos, struct error_info_node, list);
		len = strlen(node->info);
		if (len > PAGE_SIZE)
			len = PAGE_SIZE;
		if (copy_to_user((void __user *)para, node->info, len)) {
			len = 0;
			dprintk(0, "error copy to use space");
		}
		list_del(&node->list);
		break;
	}
	return len;
}

long resman_ioctl(struct file *filp, unsigned int cmd, unsigned long para)
{
	long retval = 0;
	struct resman_session *sess;

	if (_IOC_TYPE(cmd) != RESMAN_IOC_MAGIC) {
		dprintk(0, "error cmd 0x%x type 0x%x!\n", cmd, _IOC_TYPE(cmd));
		return -EINVAL;
	}
	sess = filp->private_data;
	if (!sess)
		return -EINVAL;
	switch (cmd) {
	case RESMAN_IOC_ACQUIRE_RES:
	case RESMAN_IOC_ACQUIRE_RES_V4:
		retval = resman_ioctl_acquire(sess, para);
		break;
	case RESMAN_IOC_QUERY_RES:
	case RESMAN_IOC_QUERY_RES_V4:
		retval = resman_ioctl_query(sess, para);
		break;
	case RESMAN_IOC_RELEASE_RES:
		retval = resman_ioctl_release(sess, para);
		break;
	case RESMAN_IOC_SETAPPINFO:
	case RESMAN_IOC_SETAPPINFO_V4:
		retval = resman_ioctl_setappinfo(sess, para);
		break;
	case RESMAN_IOC_SUPPORT_RES:
	case RESMAN_IOC_SUPPORT_RES_V4:
		retval = resman_ioctl_check_support(sess, para);
		break;
	case RESMAN_IOC_RELEASE_ALL:
		retval = resman_ioctl_release_all(sess, para);
		break;
	case RESMAN_IOC_LOAD_RES:
	case RESMAN_IOC_LOAD_RES_V4:
		retval = resman_ioctl_load_res(sess, para);
		break;
	case RESMAN_IOC_GET_SYS_DEBUG_LEVEL:
		retval = resman_ioctl_get_sys_debug_level(sess, para);
		break;
	case RESMAN_IOC_SET_SYS_DEBUG_LEVEL:
	case RESMAN_IOC_SET_SYS_DEBUG_LEVEL_V4:
		retval = resman_ioctl_set_sys_debug_level(sess, para);
		break;
	case RESMAN_IOC_GET_ERROR_INFO:
		retval = resman_ioctl_get_error_info(sess, para);
		break;
	case RESMAN_IOC_GET_COUNTER_INDEX:
		retval = resman_ioctl_get_counter_index(sess, para);
		break;
	case RESMAN_IOC_GET_VERSION_V4:
		retval = resman_ioctl_get_version(sess, para);
		break;
	case RESMAN_IOC_PARAMETER_SET_INFO_CTL_V4:
		retval = resman_ioctl_parameter_set_ctl(sess, para);
		break;
	default:
		retval = -EINVAL;
		break;
	}
	return retval;
}

#ifdef CONFIG_COMPAT
static long resman_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	return resman_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int resman_close(struct inode *inode, struct file *filp)
{
	struct resman_session *sess;

	sess = filp->private_data;
	if (sess) {
		dprintk(2, "%d close, proc:%s/%d/%d\n",
			sess->id,
			sess->comm,
			sess->tgid,
			sess->pid);
		resman_close_session(sess);
	}
	filp->private_data = NULL;
	return 0;
}

const struct file_operations resman_fops = {
	.owner = THIS_MODULE,
	.open = resman_open,
	.read = resman_read,
	.poll = resman_poll,
	.release = resman_close,
	.unlocked_ioctl = resman_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = resman_compat_ioctl,
#endif
};

/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */
static struct class_attribute resman_class_attrs[] = {
	__ATTR_RO(resman_usage),
	__ATTR_RW(resman_config),
	__ATTR_RO(resman_ver),
	__ATTR_RO(resman_res),
	__ATTR_RO(res_report),
	__ATTR_RW(res_sys_debug),
	__ATTR_NULL
};

static void remove_resmansub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; resman_class_attrs[i].attr.name; i++)
		class_remove_file(class, &resman_class_attrs[i]);
}

static void create_resmansub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; resman_class_attrs[i].attr.name; i++) {
		if (class_create_file(class, &resman_class_attrs[i]) < 0)
			break;
	}
}

static struct module_debug_node *resman_get_debug_module(const char *module)
{
	struct module_debug_node *node = NULL;
	struct list_head *pos = NULL, *tmp = NULL;

	if (module) {
		list_for_each_safe(pos, tmp, &debug_head) {
			node = list_entry(pos, struct module_debug_node, list);
			if (node && node->module && strlen(module) == strlen(node->module)) {
				if (!memcmp(module, node->module, strlen(module)))
					break;
			}
			node = NULL;
		}
	}

	return node;
}

int resman_register_debug_callback(const char *module, debug_callback callback)
{
	int res = 0;
	struct module_debug_node *node = NULL;

	if (!module || !callback)
		return -EINVAL;

	mutex_lock(&debug_lock);
	node = resman_get_debug_module(module);
	if (!node) {
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			dprintk(0, "failed allocate debug node for %s\n", module);
			res = -ENOMEM;
			goto error1;
		}

		node->module = kzalloc(strlen(module) + 1, GFP_KERNEL);
		if (!node->module) {
			dprintk(0, "failed allocate module for %s\n", module);
			res = -ENOMEM;
			goto error;
		}

		memcpy(node->module, module, strlen(module));
		node->callback = callback;
		list_add_tail(&node->list, &debug_head);
	}
	mutex_unlock(&debug_lock);

	return 0;
error:
	kfree(node);
error1:
	mutex_unlock(&debug_lock);
	return res;
}
EXPORT_SYMBOL(resman_register_debug_callback);

int resman_remove_debug_callback(const char *module)
{
	struct module_debug_node *node = NULL;

	if (!module)
		return -EINVAL;

	mutex_lock(&debug_lock);
	node = resman_get_debug_module(module);
	if (node) {
		list_del(&node->list);
		kfree(node->module);
		kfree(node);
	}
	mutex_unlock(&debug_lock);

	return 0;
}
EXPORT_SYMBOL(resman_remove_debug_callback);

int resman_notify_error_info(const char *info)
{
	int res = 0;
	struct error_info_node *node = NULL;
	struct list_head *pos = NULL, *tmp = NULL;
	struct resman_session *sess = NULL;

	if (!info)
		return -EINVAL;
	dprintk(1, "notify error info = %s\n", info);
	mutex_lock(&error_info_lock);
	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		dprintk(0, "failed allocate error info node for %s\n", info);
		res = -ENOMEM;
		goto error1;
	}
	node->info = kzalloc(strlen(info) + 1, GFP_KERNEL);
	if (!node->info) {
		dprintk(0, "failed allocate info for %s\n", info);
		res = -ENOMEM;
		goto error;
	}
	memcpy(node->info, info, strlen(info));
	list_add_tail(&node->list, &error_info_head);
	mutex_unlock(&error_info_lock);

	mutex_lock(&sessions_lock);
	list_for_each_safe(pos, tmp, &sessions_head) {
		sess = list_entry(pos, struct resman_session, list);
		if (sess) {
			resman_send_event(sess, RESMAN_EVENT_ERRORINFO);
			wake_up_interruptible(&sess->wq_event);
		}
	}
	mutex_unlock(&sessions_lock);
	return 0;
error:
	kfree(node);
error1:
	mutex_unlock(&error_info_lock);
	return res;
}
EXPORT_SYMBOL(resman_notify_error_info);

int __init resman_init(void)
{
	int result;
	struct device *resman_dev;

	result = alloc_chrdev_region(&resman_devno, 0, 1, DEVICE_NAME);

	if (result < 0) {
		dprintk(0, "failed to allocate amresource_mgr dev region\n");
		result = -ENODEV;
		return result;
	}

	resman_class = class_create(DEVICE_CLASS_NAME);

	if (IS_ERR(resman_class)) {
		result = PTR_ERR(resman_class);
		goto fail1;
	}
	create_resmansub_attrs(resman_class);

	resman_cdev = kmalloc(sizeof(*resman_cdev), GFP_KERNEL);
	if (!resman_cdev) {
		result = -ENOMEM;
		goto fail2;
	}
	cdev_init(resman_cdev, &resman_fops);
	resman_cdev->owner = THIS_MODULE;
	result = cdev_add(resman_cdev, resman_devno, 1);
	if (result) {
		dprintk(0, "failed to add cdev\n");
		goto fail3;
	}
	resman_dev = device_create(resman_class,
				  NULL, MKDEV(MAJOR(resman_devno), 0),
				  NULL, DEVICE_NAME);

	if (IS_ERR(resman_dev)) {
		pr_err("Can't create %s device\n", DEVICE_NAME);
		result = PTR_ERR(resman_dev);
		goto fail4;
	}
	all_resource_init();
	dprintk(1, "%s init success\n", DEVICE_NAME);

	return 0;
fail4:
	cdev_del(resman_cdev);
fail3:
	kfree(resman_cdev);
fail2:
	remove_resmansub_attrs(resman_class);
	class_destroy(resman_class);
fail1:
	unregister_chrdev_region(resman_devno, 1);
	return result;
}

void __exit resman_exit(void)
{
	all_resource_uninit();
	kfree(resman_configs);
	cdev_del(resman_cdev);
	kfree(resman_cdev);
	device_destroy(resman_class, MKDEV(MAJOR(resman_devno), 0));
	remove_resmansub_attrs(resman_class);
	class_destroy(resman_class);
	unregister_chrdev_region(resman_devno, 1);
	dprintk(1, "uninstall sourmanage module\n");
}

MODULE_LICENSE("GPL");
