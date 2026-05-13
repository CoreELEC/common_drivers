// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/amlogic/tee_drv.h>
#endif

#if IS_BUILTIN(CONFIG_AMLOGIC_MEDIA_MODULE)
#include <linux/amlogic/aml_cma.h>
#endif

#define DEFAULT_SECURE_CUSTOM_ALLOC_MULTI_NUM (2)
#define DEFAULT_SECURE_SCALING_FACTOR_NUMERATOR (3)
#define DEFAULT_SECURE_SCALING_FACTOR_DENOMINATOR (2)

static int dmabuf_manage_debug = 1;
module_param(dmabuf_manage_debug, int, 0644);

static u32 dmabuf_manage_version;

static u32 secure_vdec_config = 0x7C;

static u32 secure_buf_num_of_channel;
static u32 secure_stand_heap_version;
static u32 force_secmem_v3;
static u32 secure_custom_size_alloc_multiple = DEFAULT_SECURE_CUSTOM_ALLOC_MULTI_NUM;
static u32 secure_scaling_factor_numerator = DEFAULT_SECURE_SCALING_FACTOR_NUMERATOR;
static u32 secure_scaling_factor_denominator = DEFAULT_SECURE_SCALING_FACTOR_DENOMINATOR;

#define  DEVICE_NAME "secmem"
#define  CLASS_NAME  "dmabuf_manage"

#define CONFIG_PATH "media.dmabuf_manage"
#define CONFIG_PREFIX "media"

#define dprintk(level, fmt, arg...)						\
	do {									\
		if (dmabuf_manage_debug >= (level))						\
			pr_info(CLASS_NAME ": %s: " fmt, __func__, ## arg);\
	} while (0)

#define pr_dbg(fmt, args ...)  dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...) dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...) dprintk(8, fmt, ## args)
#define pr_enter() pr_inf("enter")

#define UHD_SIZE_MB (12 * SZ_1M)
#define ALLOC_ALIGN_SIZE (4096)
#define SECMEM_ERROR_OUT_OF_MEMORY (0x1006)

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
#define DMABUF_MANAGE_BLOCK_MIN_SIZE_KB				(256 * 1024)
#define DEFAULT_SECURE_BUFFER_NUM_OF_CHANNEL				8

#define TA_SECMEM_CMD_GET_NO_HEAD_SUPPORT			2
#define TA_SECMEM_V2_CMD_INIT						294
#define TA_SECMEM_V2_CMD_MEM_CREATE					251
#define TA_SECMEM_V2_CMD_MEM_FREE					258
#define TA_SECMEM_V2_CMD_CLOSE						266
#define TA_SECMEM_V2_CMD_PROBE_VP9_METADATA			298
#define TA_SECMEM_V2_CMD_MEM_CREATE_AND_ALLOC		283

#define TA_SECMEM_V3_CMD_INIT						3049
#define TA_SECMEM_V3_CMD_MEM_CREATE					3002
#define TA_SECMEM_V3_CMD_MEM_FREE					3009
#define TA_SECMEM_V3_CMD_CLOSE						3017
#define TA_SECMEM_V3_CMD_PROBE_VP9_METADATA				3060
#define TA_SECMEM_V3_CMD_MEM_CREATE_AND_ALLOC				3038

#define TA_SECMEM_SHM_SIZE							4096
#define TEE_CMD_PARAM_NUM							4
#define PARAM_ALIGN									32

#define SECMEM_TA_UUID UUID_INIT(0x2c1a33c0, 0x44cc, 0x11e5, \
		0xbc, 0x3b, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b)
#endif

struct kdmabuf_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

struct block_node {
	struct list_head node;
	u64 addr;
	u32 size;
	u64 phyaddr;
};

struct secure_pool_info {
	struct list_head node;
	struct list_head block_node;
	u32 version;
	u32 id_high;
	u32 id_low;
	u32 block_size;
	u32 channel_register;
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	struct tee_ioctl_open_session_arg secmem_session;
	struct tee_shm *shm_pool;
#endif
};

static long dmabuf_manage_release_channel(u32 id_high, u32 id_low);
static int dmabuf_manage_release_dmabufheap_resource(struct secure_pool_info *release_pool);
static u32 dmabuf_manage_secure_negotiated_version(void);
static int dmabuf_manage_release_es_data(struct dmabuf_manage_block *block);

static struct list_head pool_list;
static int dev_no;
static struct device *dmabuf_manage_dev;
static DEFINE_MUTEX(g_secure_pool_mutex);
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
static struct tee_context *g_secmem_context;
struct tee_ioctl_open_session_arg g_secmem_session;
struct tee_shm *g_shm_pool;
static int g_secmem_inited;
static DEFINE_MUTEX(g_no_head_mutex);

#pragma pack(push)
#pragma pack(4)

enum TEE_CMD_PARAMTYPE {
	TEE_CMD_PARAMTYPE_BUF,
	TEE_CMD_PARAMTYPE_UINT32,
	TEE_CMD_PARAMTYPE_UINT64,
	TEE_CMD_PARAMTYPE_VOID,
};

struct tee_cmdparam {
	enum TEE_CMD_PARAMTYPE type;
	union {
		struct {
			u32 buflen;
			u32 pbuf;
		} buf;
		u32 u32_value;
		u64 u64_value;
	} param;
};

#pragma pack(pop)

static int dmabuf_manage_tee_pack_u32(char *shm_data, const u32 num, u32 *poff)
{
	struct tee_cmdparam p;
	u32 off = 0;

	if (!shm_data || !poff)
		return -1;

	off = *poff;
	if (off > TA_SECMEM_SHM_SIZE - sizeof(struct tee_cmdparam))
		return -1;

	p.type = TEE_CMD_PARAMTYPE_UINT32;
	p.param.u32_value = num;
	memcpy((void *)(shm_data + off), &p, sizeof(struct tee_cmdparam));
	*poff = (off + sizeof(struct tee_cmdparam) + PARAM_ALIGN) & ~(PARAM_ALIGN - 1);

	return 0;
}

static int dmabuf_manage_tee_unpack_u32(const char *shm, u32 *num, u32 *poff)
{
	struct tee_cmdparam p;
	u32 off = 0;

	if (!shm || !poff || !num)
		return -1;

	off = *poff;
	if (off > TA_SECMEM_SHM_SIZE - sizeof(struct tee_cmdparam))
		return -1;

	memset((void *)&p, 0, sizeof(struct tee_cmdparam));
	memcpy((void *)&p, (void *)(shm + off), sizeof(struct tee_cmdparam));
	if (p.type != TEE_CMD_PARAMTYPE_UINT32) {
		pr_error("error param type %d", p.type);
		return -1;
	}

	*num = p.param.u32_value;
	*poff = (off + sizeof(struct tee_cmdparam) + PARAM_ALIGN) & ~(PARAM_ALIGN - 1);

	return 0;
}

static int dmabuf_manage_tee_pack_u64(char *shm_data, const u64 num, u32 *poff)
{
	struct tee_cmdparam p;
	u32 off = 0;

	if (!shm_data || !poff)
		return -1;

	off = *poff;
	if (off > TA_SECMEM_SHM_SIZE - sizeof(struct tee_cmdparam))
		return -1;

	p.type = TEE_CMD_PARAMTYPE_UINT64;
	p.param.u64_value = num;
	memcpy((void *)(shm_data + off), &p, sizeof(struct tee_cmdparam));
	*poff = (off + sizeof(struct tee_cmdparam) + PARAM_ALIGN) & ~(PARAM_ALIGN - 1);

	return 0;
}

static int dmabuf_manage_tee_unpack_u64(const char *shm, u64 *num, u32 *poff)
{
	struct tee_cmdparam p;
	u32 off = 0;

	if (!shm || !poff || !num)
		return -1;

	off = *poff;
	if (off > TA_SECMEM_SHM_SIZE - sizeof(struct tee_cmdparam))
		return -1;

	memcpy((void *)&p, (void *)(shm + off), sizeof(struct tee_cmdparam));
	if (p.type != TEE_CMD_PARAMTYPE_UINT64) {
		pr_error("error param type %d", p.type);
		return -1;
	}

	*num = p.param.u64_value;
	*poff = (off + sizeof(struct tee_cmdparam) + PARAM_ALIGN) & ~(PARAM_ALIGN - 1);

	return 0;
}

static int dmabuf_manage_tee_unpack_buf(const char *shm, char *pbuf_addr,
	u32 *size, u32 *poff)
{
	struct tee_cmdparam p;
	u32 off = 0;

	if (!shm || !pbuf_addr || !size || !poff)
		return -1;

	off = *poff;
	if (off > TA_SECMEM_SHM_SIZE - sizeof(struct tee_cmdparam))
		return -1;

	memcpy((void *)&p, (void *)(shm + off), sizeof(struct tee_cmdparam));
	if (p.type != TEE_CMD_PARAMTYPE_BUF) {
		pr_error("error param type %d", p.type);
		return -1;
	}

	*size = p.param.buf.buflen;
	if (*size > TA_SECMEM_SHM_SIZE ||
		off + sizeof(struct tee_cmdparam) + *size > TA_SECMEM_SHM_SIZE) {
		pr_error("unpack param len error, size %u, off %d", *size, off);
		return -1;
	}

	memcpy((void *)pbuf_addr,
		(void *)(shm + off + offsetof(struct tee_cmdparam, param.buf.pbuf)),
		*size);
	*poff = (off + sizeof(struct tee_cmdparam) + *size + PARAM_ALIGN)
		& ~(PARAM_ALIGN - 1);

	return 0;
}
#endif

static int dmabuf_manage_attach(struct dma_buf *dbuf, struct dma_buf_attachment *attachment)
{
	struct kdmabuf_attachment *attach = NULL;
	struct dmabuf_manage_block *block = dbuf->priv;
	struct sg_table *sgt = NULL;
	struct page *page = NULL;
	phys_addr_t phys = 0;
	size_t block_size = 0;
	int ret = 0;

	pr_enter();
	if (block->extend) {
		phys = block->extend_paddr;
		block_size = block->extend_size;
	} else {
		phys = block->paddr;
		block_size = block->size;
	}

	attach = (struct kdmabuf_attachment *)
		kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach) {
		pr_error("kzalloc failed\n");
		goto error;
	}

	sgt = &attach->sgt;
	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret) {
		pr_error("No memory for sgtable");
		goto error_alloc;
	}

	page = phys_to_page(phys);
	sg_set_page(sgt->sgl, page, PAGE_ALIGN(block_size), 0);

	attach->dma_dir = DMA_NONE;
	attachment->priv = attach;
	return 0;

error_alloc:
	kfree(attach);
error:
	return 0;
}

static void dmabuf_manage_detach(struct dma_buf *dbuf,
				struct dma_buf_attachment *attachment)
{
	struct kdmabuf_attachment *attach = attachment->priv;
	struct sg_table *sgt;

	pr_enter();
	if (!attach)
		return;
	sgt = &attach->sgt;
	sg_free_table(sgt);
	kfree(attach);
	attachment->priv = NULL;
}

static struct sg_table *dmabuf_manage_map_dma_buf(struct dma_buf_attachment *attachment,
		enum dma_data_direction dma_dir)
{
	struct kdmabuf_attachment *attach = NULL;
	struct dmabuf_manage_block *block = NULL;
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	struct mutex *lock = NULL;
#endif
	struct sg_table *sgt;
	size_t block_size = 0;
	phys_addr_t phys = 0;

	pr_enter();

	if (!attachment)
		return NULL;

	attach = attachment->priv;
	if (!attach)
		return NULL;

	if (!attachment->dmabuf)
		return NULL;

	block = attachment->dmabuf->priv;
	if (!block)
		return NULL;

#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	lock = &attachment->dmabuf->lock;
#endif

	if (block->extend) {
		phys = block->extend_paddr;
		block_size = block->extend_size;
	} else {
		phys = block->paddr;
		block_size = block->size;
	}

#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	mutex_lock(lock);
#endif
	sgt = &attach->sgt;
	if (attach->dma_dir == dma_dir) {
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
		mutex_unlock(lock);
#endif
		return sgt;
	}
	sgt->sgl->dma_address = phys;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	sgt->sgl->dma_length = PAGE_ALIGN(block_size);
#else
	sgt->sgl->length = PAGE_ALIGN(block_size);
#endif
	attach->dma_dir = dma_dir;
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	mutex_unlock(lock);
#endif
	return sgt;
}

static void dmabuf_manage_unmap_dma_buf(struct dma_buf_attachment *attachment,
		struct sg_table *sgt,
		enum dma_data_direction dma_dir)
{
	pr_enter();
}

static void dmabuf_manage_buf_release(struct dma_buf *dbuf)
{
	struct dmabuf_manage_block *block = NULL;
	struct secure_vdec_channel *channel = NULL;

	pr_enter();
	block = (struct dmabuf_manage_block *)dbuf->priv;
	if (block && block->type == DMA_BUF_TYPE_DMABUF) {
		if (block->extend) {
			if (block->extend_flags & DMABUF_ALLOC_FROM_CMA)
				codec_mm_free_for_dma("dmabuf", block->extend_paddr);
		} else {
			if (block->flags & DMABUF_ALLOC_FROM_CMA)
				codec_mm_free_for_dma("dmabuf", block->paddr);
		}
	} else if (block && block->priv &&
		block->type == DMA_BUF_TYPE_SECURE_VDEC) {
		channel = (struct secure_vdec_channel *)block->priv;
		dmabuf_manage_release_channel(channel->id_high, channel->id_low);
	} else if (block && block->priv &&
		block->type == DMA_BUF_TYPE_VIDEODEC_ES) {
		dmabuf_manage_release_es_data(block);
	}

	if (block) {
		kfree(block->priv);
		kfree(block);
	}
}

static int dmabuf_manage_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	struct dmabuf_manage_block *block = NULL;
	phys_addr_t paddr = 0;
	unsigned long paddr_size = 0;

	pr_enter();
	block = (struct dmabuf_manage_block *)dbuf->priv;
	if (block->extend) {
		paddr = block->extend_paddr;
		paddr_size = block->extend_size;
	} else {
		paddr = block->paddr;
		paddr_size = block->size;
	}

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return remap_pfn_range(vma, vma->vm_start, page_to_pfn(phys_to_page(paddr)),
		paddr_size, vma->vm_page_prot);
}

static struct dma_buf_ops dmabuf_manage_ops = {
	.attach = dmabuf_manage_attach,
	.detach = dmabuf_manage_detach,
	.map_dma_buf = dmabuf_manage_map_dma_buf,
	.unmap_dma_buf = dmabuf_manage_unmap_dma_buf,
	.release = dmabuf_manage_buf_release,
	.mmap = dmabuf_manage_mmap
};

bool dmabuf_is_esbuf(struct dma_buf *dmabuf)
{
	return dmabuf->ops == &dmabuf_manage_ops;
}
EXPORT_SYMBOL(dmabuf_is_esbuf);

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
static int dmabuf_manage_secmem_ctx_match(struct tee_ioctl_version_data *ver,
	const void *data)
{
	return (ver && ver->impl_id == TEE_IMPL_ID_OPTEE);
}

static int dmabuf_manage_secmem_tee_init(void)
{
	int res = -1;
	uuid_t uuid = SECMEM_TA_UUID;

	if (!g_secmem_inited) {
		if (!g_secmem_context) {
			g_secmem_context = tee_client_open_context(NULL,
				dmabuf_manage_secmem_ctx_match, NULL, NULL);
			if (IS_ERR(g_secmem_context)) {
				pr_error("%s open context failed\n", __func__);
				return -1;
			}
		}

		memcpy(g_secmem_session.uuid, uuid.b, TEE_IOCTL_UUID_LEN);
		g_secmem_session.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
		g_secmem_session.num_params = 0;

		res = tee_client_open_session(g_secmem_context, &g_secmem_session, NULL);
		if (res < 0 || g_secmem_session.ret != TEEC_SUCCESS) {
			pr_error("%s open session ret %d, res 0x%x, origin 0x%x\n",
				__func__, res, g_secmem_session.ret, g_secmem_session.ret_origin);
			return -1;
		}

		g_shm_pool = tee_shm_alloc_kernel_buf(g_secmem_context, TA_SECMEM_SHM_SIZE);
		if (IS_ERR(g_shm_pool)) {
			pr_error("%s tee_shm_alloc failed\n", __func__);
			tee_client_close_session(g_secmem_context, g_secmem_session.session);
			return -1;
		}

		g_secmem_inited = 1;
	}

	return 0;
}
#endif

int dmabuf_manage_support_nohead(void)
{
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	int res = -1;
	u32 in_len = 0;
	char *shm_data = NULL;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };

	mutex_lock(&g_no_head_mutex);
	res = dmabuf_manage_secmem_tee_init();
	if (res)
		goto error;

	shm_data = tee_shm_get_va(g_shm_pool, 0);
	if (IS_ERR(shm_data)) {
		pr_error("%s tee_shm_get_va failed\n", __func__);
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, TA_SECMEM_CMD_GET_NO_HEAD_SUPPORT, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		goto error;
	}

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = g_shm_pool;
	param[0].u.memref.size = in_len;
	param[0].u.memref.shm_offs = 0;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	inv_arg.func = TA_SECMEM_CMD_GET_NO_HEAD_SUPPORT;
	inv_arg.session = g_secmem_session.session;
	inv_arg.num_params = TEE_CMD_PARAM_NUM;
	res = tee_client_invoke_func(g_secmem_context, &inv_arg, param);
	if (!res) {
		if (inv_arg.ret != TEEC_SUCCESS) {
			pr_error("%s invoke func failed %d 0x%x 0x%x 0x%x\n",
				__func__, res, inv_arg.ret, inv_arg.ret_origin,
				(u32)param[3].u.value.a);
			goto error;
		} else {
			if (param[3].u.value.a != TEEC_SUCCESS) {
				pr_error("%s invoke func failed %d 0x%x  0x%x 0x%x\n",
					__func__, res, inv_arg.ret, inv_arg.ret_origin,
					(u32)param[3].u.value.a);
				goto error;
			}
		}
	} else {
		goto error;
	}

	mutex_unlock(&g_no_head_mutex);
	return 1;
error:
	mutex_unlock(&g_no_head_mutex);
	return 0;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(dmabuf_manage_support_nohead);

int dmabuf_manage_vp9_probe_metadata(phys_addr_t phy, u32 size, unsigned char *meta, u32 *meta_size)
{
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	int res = -1;
	u32 in_len = 0;
	u32 out_len = 0;
	char *shm_data = NULL;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };
	u32 id = 0;

	if (!phy || !size || !meta || !meta_size || *meta_size < 40)
		return -1;

	mutex_lock(&g_no_head_mutex);

	if (force_secmem_v3 == 1)
		id = TA_SECMEM_V3_CMD_PROBE_VP9_METADATA;
	else
		id = TA_SECMEM_V2_CMD_PROBE_VP9_METADATA;

	res = dmabuf_manage_secmem_tee_init();
	if (res)
		goto error;

	shm_data = tee_shm_get_va(g_shm_pool, 0);
	if (IS_ERR(shm_data)) {
		pr_error("%s tee_shm_get_va failed\n", __func__);
		res = -EBUSY;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, id, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u64(shm_data, phy, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, size, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, *meta_size, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = g_shm_pool;
	param[0].u.memref.size = in_len;
	param[0].u.memref.shm_offs = 0;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	inv_arg.func = id;
	inv_arg.session = g_secmem_session.session;
	inv_arg.num_params = TEE_CMD_PARAM_NUM;
	res = tee_client_invoke_func(g_secmem_context, &inv_arg, param);
	if (!res) {
		if (inv_arg.ret != TEEC_SUCCESS) {
			pr_error("%s invoke func failed 0x%x 0x%x\n", __func__,
				inv_arg.ret, inv_arg.ret_origin);
			res = inv_arg.ret;
			goto error;
		} else {
			if (param[3].u.value.a != TEEC_SUCCESS) {
				pr_error("%s invoke func failed 0x%x\n",
					__func__, (u32)param[3].u.value.a);
				res = param[3].u.value.a;
				goto error;
			}
		}
	} else {
		goto error;
	}

	res = dmabuf_manage_tee_unpack_buf(shm_data, meta, meta_size, &out_len);
error:
	mutex_unlock(&g_no_head_mutex);
	return res;
#else
	return -1;
#endif
}
EXPORT_SYMBOL(dmabuf_manage_vp9_probe_metadata);

static struct dma_buf *get_dmabuf(struct dmabuf_manage_block *block,
				  unsigned long flags)
{
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &dmabuf_manage_ops;
	if (block->extend)
		exp_info.size = block->extend_size;
	else
		exp_info.size = block->size;
	exp_info.flags = flags;
	exp_info.priv = (void *)block;
	exp_info.exp_name = "dmabuf_manage";

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;
	return dbuf;
}

static long dmabuf_manage_export(unsigned long args, int extend)
{
	int ret;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;
	int fd;
	int fd_flags = O_CLOEXEC;
	struct secmem_extend_block extend_block;

	pr_enter();
	block = (struct dmabuf_manage_block *)
		kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_alloc_object;
	}
	block->extend = extend;
	if (extend) {
		ret = copy_from_user((void *)&extend_block, (void __user *)args,
			sizeof(struct secmem_extend_block));
		if (ret) {
			pr_error("copy_from_user failed\n");
			goto error_copy;
		}
		block->extend_paddr = extend_block.paddr;
		block->extend_size = extend_block.size;
		block->extend_handle = extend_block.handle;
	} else {
		ret = copy_from_user((void *)block, (void __user *)args,
			sizeof(struct secmem_block));
		if (ret) {
			pr_error("copy_from_user failed\n");
			goto error_copy;
		}
	}
	block->type = DMA_BUF_TYPE_SECMEM;
	dbuf = get_dmabuf(block, fd_flags);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_export;
	}
	fd = dma_buf_fd(dbuf, fd_flags);
	if (fd < 0) {
		pr_error("dma_buf_fd failed, ret:%d, dbuf:%px, file:%px\n",
				fd, dbuf, dbuf->file);
		goto error_fd;
	}
	return fd;
error_fd:
	dma_buf_put(dbuf);
	return fd;
error_export:
error_copy:
	kfree(block);
error_alloc_object:
	return -EFAULT;
}

static long dmabuf_manage_get_handle(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct dmabuf_manage_block *block;

	pr_enter();
	ret = copy_from_user((void *)&fd, (void __user *)args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	block = dbuf->priv;
	if (block->extend)
		res = block->handle;
	else
		res = (long)(block->handle & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long dmabuf_manage_get_phyaddr(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct dmabuf_manage_block *block;

	pr_enter();
	ret = copy_from_user((void *)&fd, (void __user *)args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	block = dbuf->priv;
	if (block->extend)
		res = block->paddr;
	else
		res = (long)(block->paddr & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long dmabuf_manage_import(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t paddr;
	struct dmabuf_manage_block *block;

	pr_enter();
	ret = copy_from_user((void *)&fd, (void __user *)args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	attach = dma_buf_attach(dbuf, dmabuf_manage_dev);
	if (IS_ERR(attach)) {
		pr_error("dma_buf_attach failed\n");
		goto error_attach;
	}
	sgt = dma_buf_map_attachment(attach, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_error("dma_buf_map_attachment failed\n");
		goto error_map;
	}
	paddr = sg_dma_address(sgt->sgl);
	block = dbuf->priv;
	if (block->extend)
		res = paddr;
	else
		res = (long)(paddr & (0xffffffff));
	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
error_map:
	dma_buf_detach(dbuf, attach);
error_attach:
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static int dmabuf_manage_export_es_data(struct dmabuf_videodec_es_data *vdecdata)
{
	struct dmabuf_videodec_es_data tmp_es_data;
	struct dmabuf_videodec_es_data *k_vdecdata;
	unsigned long long tmp_addr[32];
	int i, j;
	long res;

	if (vdecdata->data_type != DMA_BUF_VIDEODEC_COMBINE)
		return 0;

	for (i = 0; i < vdecdata->data_len; i++) {
		tmp_addr[i] = 0;
		for (j = 0; j < 8; j++) {
			tmp_addr[i] += vdecdata->data[j + i * 8];
			vdecdata->data[j + i * 8] = 0;
			if (j < 7)
				tmp_addr[i] = tmp_addr[i] << 8;
		}
	}

	for (i = 0; i < vdecdata->data_len; i++) {
		unsigned long long k_vdecdata_ptr;

		res = copy_from_user((void *)&tmp_es_data,
			(void __user *)((uintptr_t)tmp_addr[i]), sizeof(tmp_es_data));
		if (res) {
			pr_error("copy_from_user failed %d\n", i);
			return -1;
		}
		k_vdecdata = kzalloc(sizeof(*k_vdecdata), GFP_KERNEL);
		if (!k_vdecdata) {
			pr_error("kmalloc failed\n");
			return -1;
		}
		memcpy(k_vdecdata, &tmp_es_data, sizeof(*k_vdecdata));
		k_vdecdata_ptr = (uintptr_t)k_vdecdata;
		vdecdata->data[0 + i * 8] = (k_vdecdata_ptr >> 56) & 0xff;
		vdecdata->data[1 + i * 8] = (k_vdecdata_ptr >> 48) & 0xff;
		vdecdata->data[2 + i * 8] = (k_vdecdata_ptr >> 40) & 0xff;
		vdecdata->data[3 + i * 8] = (k_vdecdata_ptr >> 32) & 0xff;
		vdecdata->data[4 + i * 8] = (k_vdecdata_ptr >> 24) & 0xff;
		vdecdata->data[5 + i * 8] = (k_vdecdata_ptr >> 16) & 0xff;
		vdecdata->data[6 + i * 8] = (k_vdecdata_ptr >> 8) & 0xff;
		vdecdata->data[7 + i * 8] = k_vdecdata_ptr & 0xff;
	}

	return 0;
}

static int dmabuf_manage_release_es_data(struct dmabuf_manage_block *block)
{
	struct dmabuf_videodec_es_data *vdecdata;
	struct dmabuf_videodec_es_data *k_vdecdata_ptr;
	unsigned long long tmp_addr = 0;
	int i, j;

	if (!block)
		return 0;

	if (block->type != DMA_BUF_TYPE_VIDEODEC_ES)
		return 0;

	vdecdata = (struct dmabuf_videodec_es_data *)block->priv;
	if (vdecdata->data_type != DMA_BUF_VIDEODEC_COMBINE)
		return 0;

	for (i = 0; i < vdecdata->data_len; i++) {
		tmp_addr = 0;
		for (j = 0; j < 8; j++) {
			tmp_addr += vdecdata->data[j + i * 8];
			if (j < 7)
				tmp_addr = tmp_addr << 8;
		}
		k_vdecdata_ptr = (struct dmabuf_videodec_es_data *)((uintptr_t)tmp_addr);
		kfree(k_vdecdata_ptr);
	}

	return 0;
}

static long dmabuf_manage_export_dmabuf(unsigned long args)
{
	long res = -EFAULT;
	struct dmabuf_manage_block *block;
	struct dmabuf_manage_buffer info;
	struct dmabuf_videodec_es_data *vdecdata;
	struct dma_buf *dbuf;
	int fd = -1;
	int fd_flags = O_CLOEXEC;

	pr_enter();
	res = copy_from_user((void *)&info, (void __user *)args, sizeof(info));
	if (res) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	if (info.type == DMA_BUF_TYPE_INVALID) {
		pr_error("unknown dma buf type\n");
		goto error_copy;
	}
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_copy;
	}
	switch (info.type) {
	case DMA_BUF_TYPE_SECMEM:
		fd_flags = O_CLOEXEC;
		break;
	case DMA_BUF_TYPE_DMABUF:
		fd_flags = O_RDWR | O_CLOEXEC;
		break;
	case DMA_BUF_TYPE_VIDEODEC_ES:
		fd_flags = O_RDWR | O_CLOEXEC;
		vdecdata = kzalloc(sizeof(*vdecdata), GFP_KERNEL);
		if (!vdecdata) {
			pr_error("kmalloc failed\n");
			goto error_alloc_object;
		}
		memcpy(vdecdata, &info.buffer.vdecdata, sizeof(*vdecdata));
		block->priv = vdecdata;
		if (dmabuf_manage_export_es_data(vdecdata)) {
			pr_error("export_es_data failed\n");
			goto error_alloc_object;
		}
		break;
	default:
		block->priv = NULL;
		break;
	}
	block->paddr = info.paddr;
	block->size = PAGE_ALIGN(info.size);
	block->handle = info.handle;
	block->type = info.type;
	dbuf = get_dmabuf(block, fd_flags);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_alloc_object;
	}
	fd = dma_buf_fd(dbuf, fd_flags);
	if (fd < 0) {
		pr_error("dma_buf_fd failed, ret:%d, dbuf:%px, file:%px\n",
				fd, dbuf, dbuf->file);
		goto error_fd;
	}
	return fd;
error_fd:
	dma_buf_put(dbuf);
	return fd;
error_alloc_object:
	dmabuf_manage_release_es_data(block);
	kfree(block->priv);
	kfree(block);
error_copy:
	return res;
}

static long dmabuf_manage_extend_export_dmabuf(unsigned long args)
{
	long res = -EFAULT;
	struct dmabuf_manage_block *block;
	struct dmabuf_manage_extend_buffer info;
	struct dmabuf_videodec_es_data *vdecdata;
	struct dma_buf *dbuf;
	int fd = -1;
	int fd_flags = O_CLOEXEC;

	pr_enter();
	res = copy_from_user((void *)&info, (void __user *)args, sizeof(info));
	if (res) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	if (info.type == DMA_BUF_TYPE_INVALID) {
		pr_error("unknown dma buf type\n");
		goto error_copy;
	}
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_copy;
	}
	switch (info.type) {
	case DMA_BUF_TYPE_SECMEM:
		fd_flags = O_CLOEXEC;
		break;
	case DMA_BUF_TYPE_DMABUF:
		fd_flags = O_RDWR | O_CLOEXEC;
		break;
	case DMA_BUF_TYPE_VIDEODEC_ES:
		fd_flags = O_RDWR | O_CLOEXEC;
		vdecdata = kzalloc(sizeof(*vdecdata), GFP_KERNEL);
		if (!vdecdata) {
			pr_error("kmalloc failed\n");
			goto error_alloc_object;
		}
		memcpy(vdecdata, &info.buffer.vdecdata, sizeof(*vdecdata));
		block->priv = vdecdata;
		if (dmabuf_manage_export_es_data(vdecdata)) {
			pr_error("export_es_data failed\n");
			goto error_alloc_object;
		}
		break;
	default:
		block->priv = NULL;
		break;
	}
	block->extend = 1;
	block->extend_paddr = info.paddr;
	block->extend_size = PAGE_ALIGN(info.size);
	block->extend_handle = info.handle;
	block->type = info.type;
	dbuf = get_dmabuf(block, fd_flags);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_alloc_object;
	}
	fd = dma_buf_fd(dbuf, fd_flags);
	if (fd < 0) {
		pr_error("dma_buf_fd failed\n");
		goto error_fd;
	}
	return fd;
error_fd:
	dma_buf_put(dbuf);
error_alloc_object:
	dmabuf_manage_release_es_data(block);
	kfree(block->priv);
	kfree(block);
error_copy:
	return -EFAULT;
}

static long dmabuf_manage_get_dmabufinfo(unsigned long args)
{
	struct dmabuf_manage_buffer info;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;
	int fd = -1;

	pr_enter();
	memset(&info, 0, sizeof(struct dmabuf_manage_buffer));
	if (copy_from_user((void *)&info, (void __user *)args, sizeof(info))) {
		pr_error("copy_from_user failed\n");
		goto error;
	}
	if (info.type <= DMA_BUF_TYPE_INVALID || info.type >= DMA_BUF_TYPE_MAX)
		goto error;

	fd = (int)info.fd;
	if (fd < 0)
		goto error;
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf))
		goto error;
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = dbuf->priv;
		if (block->type != info.type)
			goto error_fd;
		info.paddr = block->paddr;
		info.size = block->size;
		info.handle = block->handle;
		switch (info.type) {
		case DMA_BUF_TYPE_VIDEODEC_ES:
			memcpy(&info.buffer.vdecdata, block->priv,
				sizeof(struct dmabuf_videodec_es_data));
			break;
		default:
			break;
		}
		if (copy_to_user((void __user *)args, &info, sizeof(info))) {
			pr_error("error copy to use space");
			goto error_fd;
		}
	}
	dma_buf_put(dbuf);
	return 0;
error_fd:
	dma_buf_put(dbuf);
error:
	return -EFAULT;
}

static long dmabuf_manage_extend_get_dmabufinfo(unsigned long args)
{
	struct dmabuf_manage_extend_buffer info;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;

	pr_enter();
	if (copy_from_user((void *)&info, (void __user *)args, sizeof(info))) {
		pr_error("copy_from_user failed\n");
		goto error;
	}
	if (info.type == DMA_BUF_TYPE_INVALID)
		goto error;
	dbuf = dma_buf_get(info.fd);
	if (IS_ERR(dbuf))
		goto error;
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = dbuf->priv;
		if (block->type != info.type)
			goto error;
		info.paddr = block->extend_paddr;
		info.size = block->extend_size;
		info.handle = block->extend_handle;
		switch (info.type) {
		case DMA_BUF_TYPE_VIDEODEC_ES:
			memcpy(&info.buffer.vdecdata, block->priv,
				sizeof(struct dmabuf_videodec_es_data));
			break;
		default:
			break;
		}
		if (copy_to_user((void __user *)args, &info, sizeof(info))) {
			pr_error("error copy to use space");
			goto error_fd;
		}
	}
	dma_buf_put(dbuf);
	return 0;
error_fd:
	dma_buf_put(dbuf);
error:
	return -EFAULT;
}

static int dmabuf_manage_alloc_dmabuf(unsigned long args)
{
	int res = -EFAULT;
	struct dmabuf_manage_buffer info;
	int fd_flags = O_RDWR | O_CLOEXEC;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;
	int alloc_page = 0;

	pr_enter();
	memset(&info, 0, sizeof(struct dmabuf_manage_buffer));
	res = copy_from_user((void *)&info, (void __user *)args, sizeof(info));
	if (res)
		goto error_copy;
	if ((int)info.size <= 0 || info.size % ALLOC_ALIGN_SIZE != 0) {
		pr_error("Invalid size %d", info.size);
		goto error_copy;
	}
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_copy;
	}

	alloc_page = info.size / PAGE_SIZE;
	if (alloc_page < 0) {
		pr_error("Error size %d", info.size);
		goto error_alloc_object;
	} else if (alloc_page == 0) {
		alloc_page = 1;
	}
	block->paddr = codec_mm_alloc_for_dma("dmabuf", alloc_page,
		PAGE_SHIFT, CODEC_MM_FLAGS_DMA);
	if (block->paddr <= 0)
		goto error_alloc_object;
	block->size = PAGE_ALIGN(info.size);
	block->handle = info.handle;
	block->type = info.type;
	block->flags |= DMABUF_ALLOC_FROM_CMA;
	dbuf = get_dmabuf(block, fd_flags);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_alloc;
	}
	res = dma_buf_fd(dbuf, fd_flags);
	if (res < 0) {
		pr_error("dma_buf_fd failed\n");
		goto error_fd;
	}
	codec_mm_attach_dma_buf(dbuf, block->paddr);
	return res;
error_fd:
	dma_buf_put(dbuf);
error_alloc:
	codec_mm_free_for_dma("dmabuf", block->paddr);
error_alloc_object:
	kfree(block);
error_copy:
	return -EFAULT;
}

static int dmabuf_manage_extend_alloc_dmabuf(unsigned long args)
{
	int res = -EFAULT;
	struct dmabuf_manage_extend_buffer info;
	int fd_flags = O_RDWR | O_CLOEXEC;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;
	int alloc_page = 0;

	pr_enter();
	res = copy_from_user((void *)&info, (void __user *)args, sizeof(info));
	if (res)
		goto error_copy;
	if ((int)info.size <= 0 || info.size % ALLOC_ALIGN_SIZE != 0) {
		pr_error("Invalid size %lld", info.size);
		goto error_copy;
	}
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_copy;
	}
	block->extend = 1;

	alloc_page = info.size / PAGE_SIZE;
	if (alloc_page < 0) {
		pr_error("Error size %lld", info.size);
		goto error_alloc_object;
	} else if (alloc_page == 0) {
		alloc_page = 1;
	}
	block->extend_paddr = codec_mm_alloc_for_dma("dmabuf", alloc_page,
		PAGE_SHIFT, CODEC_MM_FLAGS_DMA);
	if (block->extend_paddr <= 0)
		goto error_alloc_object;
	block->extend_size = PAGE_ALIGN(info.size);
	block->extend_handle = info.handle;
	block->type = info.type;
	block->extend_flags |= DMABUF_ALLOC_FROM_CMA;
	dbuf = get_dmabuf(block, fd_flags);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_alloc;
	}
	res = dma_buf_fd(dbuf, fd_flags);
	if (res < 0) {
		pr_error("dma_buf_fd failed\n");
		goto error_fd;
	}
	return res;
error_fd:
	dma_buf_put(dbuf);
error_alloc:
	codec_mm_free_for_dma("dmabuf", block->extend_paddr);
error_alloc_object:
	kfree(block);
error_copy:
	return -EFAULT;
}

unsigned int dmabuf_manage_get_type(struct dma_buf *dbuf)
{
	int ret = DMA_BUF_TYPE_INVALID;
	struct dmabuf_manage_block *block;

	if (!dbuf) {
		pr_dbg("acquire dma_buf failed");
		goto error;
	}
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = (struct dmabuf_manage_block *)dbuf->priv;
		if (block)
			ret = block->type;
	}
error:
	return ret;
}
EXPORT_SYMBOL(dmabuf_manage_get_type);

void *dmabuf_manage_get_info(struct dma_buf *dbuf, unsigned int type)
{
	void *buf = NULL;
	struct dmabuf_manage_block *block;

	if (type == DMA_BUF_TYPE_INVALID)
		goto error;
	if (!dbuf) {
		pr_dbg("acquire dma_buf failed");
		goto error;
	}
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = (struct dmabuf_manage_block *)dbuf->priv;
		switch (block->type) {
		case DMA_BUF_TYPE_SECMEM:
			buf = block;
			break;
		case DMA_BUF_TYPE_DMABUF:
			buf = block;
			break;
		case DMA_BUF_TYPE_VIDEODEC_ES:
			buf = block->priv;
			break;
		default:
			break;
		}
	}
error:
	return buf;
}
EXPORT_SYMBOL(dmabuf_manage_get_info);

static struct secure_pool_info *dmabuf_manage_get_secure_vdec_pool(u32 id_high,
	u32 id_low)
{
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;
	struct secure_pool_info *pool = NULL;

	if (!list_empty(&pool_list)) {
		list_for_each_safe(pos, tmp, &pool_list) {
			pool = list_entry(pos, struct secure_pool_info, node);
			if (pool && pool->id_high == id_high && pool->id_low == id_low)
				return pool;
		}
	}

	return NULL;
}

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
static int dmabuf_manage_secure_v2_init(struct secure_pool_info *pool, u32 flags, u32 size)
{
	int res = 0;
	u32 in_len = 0;
	u32 config = 0;
	u32 tvp_flag = flags & 0x0F;
	u32 decoder_flag = (flags >> 4) & 0x0F;
	u32 vd_index = (flags >> 9) & 0x0F;
	char *shm_data = NULL;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };
	u32 allocsize = 0;
	u32 buf_num = 0;

	if (!pool || !g_secmem_context)
		return -1;

	if (!pool->shm_pool)
		return -1;

	config = secure_vdec_config;
	if ((decoder_flag & 0x3) == 0x3)
		config |= 0x400000;

	if ((decoder_flag & 0x8) == 0x8)
		config |= 0x2;

	if (tvp_flag == 0x2) {
		config |= 0x100;
	} else if (tvp_flag == 0x4) {
		if (secure_buf_num_of_channel == 0)
			buf_num = DEFAULT_SECURE_BUFFER_NUM_OF_CHANNEL;
		else
			buf_num = secure_buf_num_of_channel;

		config |= (1 << 28);
		config |= (1 << 8);
		allocsize = pool->block_size * buf_num;
	} else if (tvp_flag == 0x03) {
		config |= (1 << 28);
		config |= (1 << 8);
		allocsize = size;
	}

	config |= (vd_index << 11);
	config |= 0x10000;

	shm_data = tee_shm_get_va(pool->shm_pool, 0);
	if (IS_ERR(shm_data)) {
		pr_error("%s tee_shm_get_va failed\n", __func__);
		res = -EBUSY;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, TA_SECMEM_V2_CMD_INIT, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, 1, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, config, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, 0, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, allocsize, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, pool->version, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, pool->id_high, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, pool->id_low, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = pool->shm_pool;
	param[0].u.memref.size = in_len;
	param[0].u.memref.shm_offs = 0;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	inv_arg.func = TA_SECMEM_V2_CMD_INIT;
	inv_arg.session = pool->secmem_session.session;
	inv_arg.num_params = TEE_CMD_PARAM_NUM;
	res = tee_client_invoke_func(g_secmem_context, &inv_arg, param);
	if (!res) {
		if (inv_arg.ret != TEEC_SUCCESS) {
			pr_error("%s invoke func failed %d  0x%x 0x%x 0x%x\n",
				__func__, res, inv_arg.ret, inv_arg.ret_origin,
				(u32)param[3].u.value.a);
			res = inv_arg.ret;
		} else {
			if (param[3].u.value.a != TEEC_SUCCESS) {
				pr_error("%s invoke func failed %d 0x%x 0x%x 0x%x\n",
					__func__, res, inv_arg.ret, inv_arg.ret_origin,
					(u32)param[3].u.value.a);
				res = param[3].u.value.a;
			}
		}
	}

error:
	return res;
}

static int dmabuf_manage_secure_v3_init(struct secure_pool_info *pool, u32 flags, u32 size)
{
	int res = 0;
	u32 in_len = 0;
	u64 config = 0;
	u64 tvp_flag = flags & 0x0F;
	u64 decoder_flag = (flags >> 4) & 0x0F;
	u64 vd_index = (flags >> 9) & 0x0F;
	char *shm_data = NULL;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };
	u32 allocsize = 0;
	u32 buf_num = 0;

	if (!pool || !g_secmem_context)
		return -1;

	if (!pool->shm_pool)
		return -1;

	config = secure_vdec_config;
	if ((decoder_flag & 0x3) == 0x3)
		config |= 0x400000;

	if ((decoder_flag & 0x8) == 0x8)
		config |= 0x2;

	if (tvp_flag == 0x2) {
		config |= 0x100;
	} else if (tvp_flag == 0x4) {
		if (secure_buf_num_of_channel == 0)
			buf_num = DEFAULT_SECURE_BUFFER_NUM_OF_CHANNEL;
		else
			buf_num = secure_buf_num_of_channel;

		config |= (1 << 28);
		config |= (1 << 8);
		allocsize = pool->block_size * buf_num;
	} else if (tvp_flag == 0x03) {
		config |= (1 << 28);
		config |= (1 << 8);
		allocsize = size;
	}

	config |= (vd_index << 11);
	config |= 0x10000;

	shm_data = tee_shm_get_va(pool->shm_pool, 0);
	if (IS_ERR(shm_data)) {
		pr_error("%s tee_shm_get_va failed\n", __func__);
		res = -EBUSY;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, TA_SECMEM_V3_CMD_INIT, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u64(shm_data, 1, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u64(shm_data, config, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u64(shm_data, 0, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u64(shm_data, allocsize, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, pool->version, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, pool->id_high, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, pool->id_low, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = pool->shm_pool;
	param[0].u.memref.size = in_len;
	param[0].u.memref.shm_offs = 0;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	inv_arg.func = TA_SECMEM_V3_CMD_INIT;
	inv_arg.session = pool->secmem_session.session;
	inv_arg.num_params = TEE_CMD_PARAM_NUM;
	res = tee_client_invoke_func(g_secmem_context, &inv_arg, param);
	if (!res) {
		if (inv_arg.ret != TEEC_SUCCESS) {
			pr_error("%s invoke func failed %d 0x%x 0x%x 0x%x\n",
				__func__, res, inv_arg.ret, inv_arg.ret_origin,
				(u32)param[3].u.value.a);
			res = inv_arg.ret;
		} else {
			if (param[3].u.value.a != TEEC_SUCCESS) {
				pr_error("%s invoke func failed  %d 0x%x 0x%x 0x%x\n",
					__func__, res, inv_arg.ret, inv_arg.ret_origin,
					(u32)param[3].u.value.a);
				res = param[3].u.value.a;
			}
		}
	}

error:
	return res;
}

static int dmabuf_manage_secmem_pool_init(u32 id_high, u32 id_low, u32 block_size, u32 version)
{
	int res = 1;
	struct secure_pool_info *pool = NULL;
	uuid_t uuid = SECMEM_TA_UUID;
	unsigned int tvp_set = 0;
	unsigned int codec_flags = 0;
	unsigned int vd_index = 1;
	unsigned int custom_size = 0;
	unsigned int i = 0;

	if (version < SECURE_HEAP_USER_TA_VERSION)
		goto error;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		goto error;

	pool->version = version;
	pool->id_high = id_high;
	pool->id_low = id_low;
	pool->block_size = block_size;
	if (block_size <= DMABUF_MANAGE_BLOCK_MIN_SIZE_KB) {
		codec_flags = 0x3;
		codec_flags |= 0x8;
	} else if (block_size <= 2 * SZ_1M) {
		tvp_set = 1;
	} else if (block_size >= 6 * SZ_1M) {
		tvp_set = 2;
	} else {
		tvp_set = 3;
		custom_size = PAGE_ALIGN(block_size * secure_custom_size_alloc_multiple);
	}

	if (version >= SECURE_HEAP_STAND_POOL_VERSION)
		tvp_set = 4;

	if (!g_secmem_context) {
		g_secmem_context = tee_client_open_context(NULL,
			dmabuf_manage_secmem_ctx_match, NULL, NULL);
		if (IS_ERR(g_secmem_context)) {
			pr_error("%s open context failed\n", __func__);
			goto error_alloc;
		}
	}

	memcpy(pool->secmem_session.uuid, uuid.b, TEE_IOCTL_UUID_LEN);
	pool->secmem_session.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	pool->secmem_session.num_params = 0;

	res = tee_client_open_session(g_secmem_context, &pool->secmem_session, NULL);
	if (res < 0 || pool->secmem_session.ret != TEEC_SUCCESS) {
		pr_error("%s open session ret %d 0x%x  0x%x\n",
			__func__, res, pool->secmem_session.ret,
			pool->secmem_session.ret_origin);
		res = pool->secmem_session.ret;
		goto error_alloc;
	}

	pool->shm_pool = tee_shm_alloc_kernel_buf(g_secmem_context,
		TA_SECMEM_SHM_SIZE);
	if (IS_ERR(pool->shm_pool)) {
		pr_error("%s tee_shm_alloc failed\n", __func__);
		res = -ENOMEM;
		goto error_alloc_shm;
	}

	if (force_secmem_v3 == 1) {
		res = dmabuf_manage_secure_v3_init(pool,
			tvp_set | (codec_flags << 4) | (vd_index << 9), custom_size);
	} else {
		res = dmabuf_manage_secure_v2_init(pool,
			tvp_set | (codec_flags << 4) | (vd_index << 9), custom_size);
	}

	if (res == SECMEM_ERROR_OUT_OF_MEMORY) {
		if (tvp_set == 2 || tvp_set == 3) {
			if (tvp_set == 2) {
				custom_size = UHD_SIZE_MB;
				tvp_set = 3;
			}

			custom_size -= SZ_1M;
		}

		for (i = custom_size; i >= block_size *
				secure_scaling_factor_numerator /
				secure_scaling_factor_denominator;
				i -= SZ_1M) {
			if (force_secmem_v3 == 1) {
				res = dmabuf_manage_secure_v3_init(pool,
					tvp_set | (codec_flags << 4) | (vd_index << 9),
					custom_size);
			} else {
				res = dmabuf_manage_secure_v2_init(pool,
					tvp_set | (codec_flags << 4) | (vd_index << 9),
					custom_size);
			}

			if (res != SECMEM_ERROR_OUT_OF_MEMORY) {
				if (res == TEEC_SUCCESS)
					pr_inf("Retry alloc vdec pool is 0x%x. max size 0x%x\n",
						custom_size, block_size);
				break;
			}
		}
	}

	if (res) {
		pr_error("%s secure v2 init failed\n", __func__);
		goto error_open_session;
	}

	INIT_LIST_HEAD(&pool->block_node);
	list_add_tail(&pool->node, &pool_list);
	return 0;
error_open_session:
	tee_shm_free(pool->shm_pool);
error_alloc_shm:
	tee_client_close_session(g_secmem_context, pool->secmem_session.session);
error_alloc:
	kfree(pool);
error:
	return res;
}

static void dmabuf_manage_destroy_secmem_pool(struct secure_pool_info *pool)
{
	int res = 0;
	u32 in_len = 0;
	char *shm_data = NULL;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };
	u32 tee_command = TA_SECMEM_V2_CMD_CLOSE;

	if (!pool)
		return;

	if (list_empty(&pool->block_node)) {
		if (pool->shm_pool) {
			shm_data = tee_shm_get_va(pool->shm_pool, 0);
			if (IS_ERR(shm_data)) {
				pr_error("%s tee_shm_get_va failed\n", __func__);
				return;
			}

			if (force_secmem_v3 == 1)
				tee_command = TA_SECMEM_V3_CMD_CLOSE;

			res = dmabuf_manage_tee_pack_u32(shm_data, tee_command, &in_len);
			if (res) {
				pr_error("%s pass tee parameter failed\n", __func__);
				return;
			}

			param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
			param[0].u.memref.shm = pool->shm_pool;
			param[0].u.memref.size = in_len;
			param[0].u.memref.shm_offs = 0;
			param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

			inv_arg.func = tee_command;
			inv_arg.session = pool->secmem_session.session;
			inv_arg.num_params = TEE_CMD_PARAM_NUM;
			res = tee_client_invoke_func(g_secmem_context, &inv_arg, param);
			if (!res) {
				if (inv_arg.ret != TEEC_SUCCESS) {
					pr_error("failed 0x%x\n", inv_arg.ret);
					res = inv_arg.ret;
				} else {
					if (param[3].u.value.a != TEEC_SUCCESS) {
						pr_error("failed 0x%x\n", (u32)param[3].u.value.a);
						res = param[3].u.value.a;
					}
				}
			}

			if (res)
				return;

			tee_shm_free(pool->shm_pool);
			res = tee_client_close_session(g_secmem_context,
				pool->secmem_session.session);
			if (res)
				pr_error("close secmem session error 0x%x", res);
		}

		list_del(&pool->node);
		kfree(pool);
	}
}
#endif


int dmabuf_manage_secure_pool_create(u32 id_high, u32 id_low, u32 block_size,
	u32 version)
{
	int res = 0;
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	struct secure_pool_info *pool = NULL;

	mutex_lock(&g_secure_pool_mutex);
	pool = dmabuf_manage_get_secure_vdec_pool(id_high, id_low);
	if (!pool)
		res = dmabuf_manage_secmem_pool_init(id_high, id_low,
			block_size, version);

	mutex_unlock(&g_secure_pool_mutex);
#else
	res = -1;
#endif
	return res;
}

static long dmabuf_manage_register_channel(unsigned long args)
{
	struct secure_vdec_channel *channel = NULL;
	struct secure_pool_info *pool = NULL;
	int ret = 0;
	int fd = -1;
	int fd_flags = O_CLOEXEC;
	struct dmabuf_manage_block *block = NULL;
	struct dma_buf *dbuf = NULL;

	mutex_lock(&g_secure_pool_mutex);
	pr_enter();

	channel = (struct secure_vdec_channel *)
		kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel) {
		pr_error("kmalloc channel failed\n");
		goto error_alloc_channel;
	}

	ret = copy_from_user((void *)channel, (void __user *)args, sizeof(*channel));
	if (ret) {
		pr_error("copy channel information failed\n");
		goto error_copy;
	}

	pool = dmabuf_manage_get_secure_vdec_pool(channel->id_high, channel->id_low);
	if (pool) {
		pool->channel_register = 1;
		block = (struct dmabuf_manage_block *)
			kzalloc(sizeof(*block), GFP_KERNEL);
		if (!block) {
			pr_error("kmalloc failed\n");
			goto error_alloc_object;
		}

		block->size = pool->block_size;
		block->type = DMA_BUF_TYPE_SECURE_VDEC;
		block->priv = (void *)channel;

		dbuf = get_dmabuf(block, fd_flags);
		if (!dbuf) {
			pr_error("get_dmabuf failed\n");
			goto error_export;
		}

		fd = dma_buf_fd(dbuf, fd_flags);
		if (fd < 0) {
			pr_error("dma_buf_fd failed\n");
			goto error_fd;
		}
	} else {
		goto error_copy;
	}

	mutex_unlock(&g_secure_pool_mutex);
	return fd;
error_fd:
	dma_buf_put(dbuf);
error_export:
	kfree(block);
error_alloc_object:
error_copy:
	kfree(channel);
error_alloc_channel:
	mutex_unlock(&g_secure_pool_mutex);
	return -EFAULT;
}

static long dmabuf_manage_release_channel(u32 id_high, u32 id_low)
{
	struct secure_pool_info *pool = NULL;
	pr_enter();

	mutex_lock(&g_secure_pool_mutex);

	pool = dmabuf_manage_get_secure_vdec_pool(id_high, id_low);
	if (pool) {
		pool->channel_register = 0;
		dmabuf_manage_release_dmabufheap_resource(pool);
	}

	mutex_unlock(&g_secure_pool_mutex);
	return 0;
}

static void dmabuf_manage_dump_secure_pool(struct secure_pool_info *pool)
{
	u32 block_count = 0;
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;
	struct block_node *block = NULL;

	if (pool) {
		pr_inf("Pool Ver %d %d Id %d %d Block size %x\n",
			pool->version, pool->channel_register,
			pool->id_high, pool->id_low, pool->block_size);

		if (!list_empty(&pool->block_node)) {
			list_for_each_safe(pos, tmp, &pool->block_node) {
				block = list_entry(pos, struct block_node, node);
				if (block) {
					pr_inf("Block Count %x addr %llx size %x\n", block_count,
						block->addr, block->size);
					block_count++;
				}
			}
		}
	}
}

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
static int dmabuf_manage_secmem_block_alloc(struct secure_pool_info *pool, u32 size, u64 *handle,
	u64 *paddr, u32 *allocsize)
{
	int res = 0;
	u32 in_len = 0;
	u32 out_off = 0;
	char *shm_data = NULL;
	u32 handle_32 = 0;
	u32 paddr_32 = 0;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };
	u32 tee_command = TA_SECMEM_V2_CMD_MEM_CREATE;

	if (!pool || !g_secmem_context || !handle)
		return -1;

	if (!pool->shm_pool)
		return -1;

	shm_data = tee_shm_get_va(pool->shm_pool, 0);
	if (IS_ERR(shm_data)) {
		pr_error("%s tee_shm_get_va failed\n", __func__);
		res = -EBUSY;
		goto error;
	}

	if (force_secmem_v3 == 1) {
		if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION)
			tee_command = TA_SECMEM_V3_CMD_MEM_CREATE_AND_ALLOC;
		else
			tee_command = TA_SECMEM_V3_CMD_MEM_CREATE;
	} else {
		if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION)
			tee_command = TA_SECMEM_V2_CMD_MEM_CREATE_AND_ALLOC;
	}

	res = dmabuf_manage_tee_pack_u32(shm_data, tee_command, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	if (tee_command == TA_SECMEM_V2_CMD_MEM_CREATE_AND_ALLOC ||
		tee_command == TA_SECMEM_V3_CMD_MEM_CREATE_AND_ALLOC) {
		res = dmabuf_manage_tee_pack_u32(shm_data, size, &in_len);
		if (res) {
			pr_error("%s pass tee parameter failed\n", __func__);
			res = -EFAULT;
			goto error;
		}
	}

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = pool->shm_pool;
	param[0].u.memref.size = in_len;
	param[0].u.memref.shm_offs = 0;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	inv_arg.func = tee_command;
	inv_arg.session = pool->secmem_session.session;
	inv_arg.num_params = TEE_CMD_PARAM_NUM;
	res = tee_client_invoke_func(g_secmem_context, &inv_arg, param);
	if (!res) {
		if (inv_arg.ret != TEEC_SUCCESS) {
			pr_error("%s invoke func failed %d 0x%x  0x%x 0x%x\n",
				__func__, res, inv_arg.ret, inv_arg.ret_origin,
				(u32)param[3].u.value.a);
			res = inv_arg.ret;
			goto error;
		} else {
			if (param[3].u.value.a != TEEC_SUCCESS) {
				pr_error("%s invoke func failed %d 0x%x 0x%x 0x%x\n",
					__func__, res, inv_arg.ret, inv_arg.ret_origin,
					(u32)param[3].u.value.a);
				res = param[3].u.value.a;
				goto error;
			}
		}
	} else {
		goto error;
	}

	if (force_secmem_v3 == 1) {
		if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION) {
			res = dmabuf_manage_tee_unpack_u64(shm_data, handle, &out_off);
			if (!res) {
				res = dmabuf_manage_tee_unpack_u64(shm_data, paddr, &out_off);
				if (!res)
					res = dmabuf_manage_tee_unpack_u32(shm_data, allocsize,
						&out_off);
			}
		} else {
			res = dmabuf_manage_tee_unpack_u64(shm_data, handle, &out_off);
		}
	} else {
		if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION) {
			res = dmabuf_manage_tee_unpack_u32(shm_data,
				&handle_32, &out_off);
			if (!res) {
				res = dmabuf_manage_tee_unpack_u32(shm_data, &paddr_32, &out_off);
				if (!res) {
					res = dmabuf_manage_tee_unpack_u32(shm_data, allocsize,
						&out_off);
					if (!res) {
						*handle = handle_32;
						*paddr = paddr_32;
					}
				}
			}
		} else {
			res = dmabuf_manage_tee_unpack_u32(shm_data, (u32 *)handle, &out_off);
		}
	}

error:
	return res;
}

static u64 dmabuf_manage_handle2addr(struct secure_pool_info *pool, u64 handle)
{
	struct block_node *block = NULL;
	struct list_head *pos = NULL, *tmp = NULL;

	if (pool) {
		if (!list_empty(&pool->block_node)) {
			list_for_each_safe(pos, tmp, &pool->block_node) {
				block = list_entry(pos, struct block_node, node);
				if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION) {
					if (block && block->phyaddr == handle)
						return block->addr;
				} else {
					if (block && block->addr == handle)
						return block->addr;
				}
			}
		}
	}

	return 0;
}

static int dmabuf_manage_secmem_block_free(struct secure_pool_info *pool, u64 handle)
{
	int res = 0;
	u32 in_len = 0;
	char *shm_data = NULL;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };
	u32 tee_command = TA_SECMEM_V2_CMD_MEM_FREE;

	if (!pool || !g_secmem_context)
		return -1;

	if (!pool->shm_pool)
		return -1;

	shm_data = tee_shm_get_va(pool->shm_pool, 0);
	if (IS_ERR(shm_data)) {
		pr_error("%s tee_shm_get_va failed\n", __func__);
		res = -EBUSY;
		goto error;
	}

	if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION) {
		handle = dmabuf_manage_handle2addr(pool, handle);
		if (!handle) {
			pr_error("%s pass tee parameter failed\n", __func__);
			res = -EFAULT;
			goto error;
		}
	}

	if (force_secmem_v3 == 1)
		tee_command = TA_SECMEM_V3_CMD_MEM_FREE;

	res = dmabuf_manage_tee_pack_u32(shm_data, tee_command, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	if (force_secmem_v3 == 1)
		res = dmabuf_manage_tee_pack_u64(shm_data, handle, &in_len);
	else
		res = dmabuf_manage_tee_pack_u32(shm_data, (u32)handle, &in_len);
	if (res) {
		pr_error("%s pass tee parameter failed\n", __func__);
		res = -EFAULT;
		goto error;
	}

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = pool->shm_pool;
	param[0].u.memref.size = in_len;
	param[0].u.memref.shm_offs = 0;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	inv_arg.func = tee_command;
	inv_arg.session = pool->secmem_session.session;
	inv_arg.num_params = TEE_CMD_PARAM_NUM;
	res = tee_client_invoke_func(g_secmem_context, &inv_arg, param);
	if (!res) {
		if (inv_arg.ret != TEEC_SUCCESS) {
			pr_error("%s invoke func failed %d  0x%x  0x%x 0x%x\n",
				__func__, res, inv_arg.ret, inv_arg.ret_origin,
				(u32)param[3].u.value.a);
			res = inv_arg.ret;
		} else {
			if (param[3].u.value.a != TEEC_SUCCESS) {
				pr_error("%s invoke func failed  %d, 0x%x, 0x%x 0x%x\n",
					__func__, res, inv_arg.ret, inv_arg.ret_origin,
					(u32)param[3].u.value.a);
				res = param[3].u.value.a;
			}
		}
	}

error:
	return res;
}
#endif

static int dmabuf_manage_release_dmabufheap_resource(struct secure_pool_info *pool)
{
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	struct block_node *block = NULL;
	struct list_head *pos = NULL, *tmp = NULL;
	u64 handle = 0;

	if (!pool)
		return -EINVAL;

	if (!list_empty(&pool->block_node)) {
		list_for_each_safe(pos, tmp, &pool->block_node) {
			block = list_entry(pos, struct block_node, node);
			if (block) {
				if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION) {
					if (block->phyaddr)
						handle = block->phyaddr;
				} else {
					if (block->addr)
						handle = block->addr;
				}

				dmabuf_manage_secmem_block_free(pool, handle);
				list_del(&block->node);
				kfree(block);
			}
		}
	}

	dmabuf_manage_destroy_secmem_pool(pool);
#endif

	return 0;
}

u64 dmabuf_manage_secure_block_alloc(u32 id_high, u32 id_low, u32 size,
	u32 version)
{
	struct secure_pool_info *pool = NULL;
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	u64 res = 0;
	u32 allocsize = 0;
#endif
	u64 addr = 0;
	u64 phyaddr = 0;
	struct block_node *block = NULL;

	mutex_lock(&g_secure_pool_mutex);
	pool = dmabuf_manage_get_secure_vdec_pool(id_high, id_low);
	if (pool) {
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
		res = dmabuf_manage_secmem_block_alloc(pool, size, &addr, &phyaddr, &allocsize);
		if (res) {
			mutex_unlock(&g_secure_pool_mutex);
			return 0;
		}
#endif
		if (addr) {
			block = kzalloc(sizeof(*block), GFP_KERNEL);
			if (block) {
				block->addr = addr;
				block->size = size;
				block->phyaddr = phyaddr;
				list_add_tail(&block->node, &pool->block_node);
			} else {
				pr_error("No Memory for secure block\n");
			}

			if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION)
				addr = phyaddr;

		} else {
			dmabuf_manage_dump_secure_pool(pool);
		}
	} else {
		pr_error("Can't found pool for %d %d\n", id_high, id_low);
	}

	mutex_unlock(&g_secure_pool_mutex);
	return addr;
}

int dmabuf_manage_secure_block_free(u32 id_high, u32 id_low,
	u64 addr, u32 size, u32 version)
{
	struct secure_pool_info *pool = NULL;
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;
	struct block_node *block = NULL;

	if (addr == 0 || size == 0)
		return -EINVAL;

	mutex_lock(&g_secure_pool_mutex);
	pool = dmabuf_manage_get_secure_vdec_pool(id_high, id_low);
	if (!pool)
		goto free_block;

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	if (version >= SECURE_HEAP_USER_TA_VERSION)
		dmabuf_manage_secmem_block_free(pool, addr);
#endif

	if (!list_empty(&pool->block_node)) {
		list_for_each_safe(pos, tmp, &pool->block_node) {
			block = list_entry(pos, struct block_node, node);
			if (block && block->size == size) {
				if (pool->version >= SECURE_HEAP_STAND_POOL_VERSION) {
					if (block->phyaddr == addr) {
						list_del(&block->node);
						kfree(block);
					}
				} else {
					if (block->addr == addr) {
						list_del(&block->node);
						kfree(block);
					}
				}
			}
		}
	}

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	dmabuf_manage_destroy_secmem_pool(pool);
#endif

free_block:
	mutex_unlock(&g_secure_pool_mutex);
	return 0;
}

static u32 dmabuf_manage_secure_negotiated_version(void)
{
	u32 version = SECURE_HEAP_USER_TA_VERSION;
	u64 addr = codec_mm_secure_vdec_max_addr();

	if (dmabuf_manage_version >= AML_DMA_BUF_MANAGER_VERSION) {
		force_secmem_v3 = 1;
		return SECURE_HEAP_USER_TA_VERSION_EXTEND;
	}

	if (addr & 0xFFFFFFFF00000000UL) {
		force_secmem_v3 = 1;
		return SECURE_HEAP_USER_TA_VERSION_EXTEND;
	}

	return version;
}

static u32 dmabuf_manage_negotiated_version(void)
{
	u32 version = AML_DMA_BUF_MANAGER_VERSION;
	u64 addr = codec_mm_managed_max_addr();

	if (dmabuf_manage_version >= AML_DMA_BUF_MANAGER_VERSION)
		return version;

	if (addr & 0xFFFFFFFF00000000UL)
		return version;

	return 1;
}

int dmabuf_manage_get_secure_heap_version(void)
{
	int version = dmabuf_manage_secure_negotiated_version();

	if (secure_stand_heap_version >= SECURE_HEAP_STAND_POOL_VERSION)
		return secure_stand_heap_version;

	return version;
}

static int dmabuf_manage_open(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static ssize_t dmabuf_manage_read(struct file *filep, char *buffer,
			   size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static ssize_t dmabuf_manage_write(struct file *filep, const char *buffer,
				size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static int dmabuf_manage_release(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static long dmabuf_manage_ioctl(struct file *filep, unsigned int cmd,
			 unsigned long args)
{
	long ret = -EINVAL;

	pr_inf("cmd %x\n", cmd);
	switch (cmd) {
	case DMABUF_MANAGE_EXPORT_DMA:
		ret = dmabuf_manage_export(args, 0);
		break;
	case DMABUF_MANAGE_GET_HANDLE:
		ret = dmabuf_manage_get_handle(args);
		break;
	case DMABUF_MANAGE_GET_PHYADDR:
		ret = dmabuf_manage_get_phyaddr(args);
		break;
	case DMABUF_MANAGE_IMPORT_DMA:
		ret = dmabuf_manage_import(args);
		break;
	case DMABUF_MANAGE_REGISTER_CHANNEL:
		ret = dmabuf_manage_register_channel(args);
		break;
	case DMABUF_MANAGE_VERSION:
	case DMABUF_MANAGE_V_VERSION:
		ret = dmabuf_manage_negotiated_version();
		break;
	case DMABUF_MANAGE_EXPORT_DMABUF:
	case DMABUF_MANAGE_V_EXPORT_DMABUF:
		ret = dmabuf_manage_export_dmabuf(args);
		break;
	case DMABUF_MANAGE_GET_DMABUFINFO:
	case DMABUF_MANAGE_V_GET_DMABUFINFO:
		ret = dmabuf_manage_get_dmabufinfo(args);
		break;
	case DMABUF_MANAGE_ALLOCDMABUF:
	case DMABUF_MANAGE_V_ALLOCDMABUF:
		ret = dmabuf_manage_alloc_dmabuf(args);
		break;
	case DMABUF_MANAGE_EXTEND_EXPORT_DMA:
		ret = dmabuf_manage_export(args, 1);
		break;
	case DMABUF_MANAGE_EXTEND_GET_HANDLE:
		ret = dmabuf_manage_get_handle(args);
		break;
	case DMABUF_MANAGE_EXTEND_GET_PHYADDR:
		ret = dmabuf_manage_get_phyaddr(args);
		break;
	case DMABUF_MANAGE_EXTEND_IMPORT_DMA:
		ret = dmabuf_manage_import(args);
		break;
	case DMABUF_MANAGE_EXTEND_REGISTER_CHANNEL:
		ret = dmabuf_manage_register_channel(args);
		break;
	case DMABUF_MANAGE_EXTEND_EXPORT_DMABUF:
		ret = dmabuf_manage_extend_export_dmabuf(args);
		break;
	case DMABUF_MANAGE_EXTEND_GET_DMABUFINFO:
		ret = dmabuf_manage_extend_get_dmabufinfo(args);
		break;
	case DMABUF_MANAGE_EXTEND_ALLOCDMABUF:
		ret = dmabuf_manage_extend_alloc_dmabuf(args);
		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long dmabuf_manage_compat_ioctl(struct file *filep, unsigned int cmd,
				unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = dmabuf_manage_ioctl(filep, cmd, args);
	return ret;
}
#endif

const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dmabuf_manage_open,
	.read = dmabuf_manage_read,
	.write = dmabuf_manage_write,
	.release = dmabuf_manage_release,
	.unlocked_ioctl = dmabuf_manage_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = dmabuf_manage_compat_ioctl,
#endif
};

static ssize_t dmabuf_manage_dump_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;
	struct secure_pool_info *pool = NULL;

	mutex_lock(&g_secure_pool_mutex);

	if (!list_empty(&pool_list) && dmabuf_manage_debug >= 6) {
		list_for_each_safe(pos, tmp, &pool_list) {
			pool = list_entry(pos, struct secure_pool_info, node);
				dmabuf_manage_dump_secure_pool(pool);
		}
	}

	mutex_unlock(&g_secure_pool_mutex);
	return 0;
}

static ssize_t dmabuf_manage_config_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t ret;

	ret = configs_list_path_nodes(CONFIG_PATH, buf, PAGE_SIZE,
					  LIST_MODE_NODE_CMDVAL_ALL);
	return ret;
}

static ssize_t dmabuf_manage_config_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	int ret;

	ret = configs_set_prefix_path_valonpath(CONFIG_PREFIX, buf);
	if (ret < 0)
		pr_info("Unsupported config %s\n", buf);

	return size;
}

static struct mconfig dmabuf_manage_configs[] = {
	MC_PI32("secure_vdec_config", &secure_vdec_config),
	MC_PI32("dmabuf_manage_version", &dmabuf_manage_version),
	MC_PI32("secure_stand_heap_version", &secure_stand_heap_version),
	MC_PI32("secure_buf_num_of_channel", &secure_buf_num_of_channel),
	MC_PI32("force_secmem_v3", &force_secmem_v3),
	MC_PI32("secure_custom_size_alloc_multiple", &secure_custom_size_alloc_multiple),
	MC_PI32("secure_scaling_factor_numerator", &secure_scaling_factor_numerator),
	MC_PI32("secure_scaling_factor_denominator", &secure_scaling_factor_denominator)
};

static CLASS_ATTR_RO(dmabuf_manage_dump);
static CLASS_ATTR_RW(dmabuf_manage_config);

static struct attribute *dmabuf_manage_class_attrs[] = {
	&class_attr_dmabuf_manage_dump.attr,
	&class_attr_dmabuf_manage_config.attr,
	NULL
};

ATTRIBUTE_GROUPS(dmabuf_manage_class);

static struct class dmabuf_manage_class = {
	.name = CLASS_NAME,
	.class_groups = dmabuf_manage_class_groups,
};

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
static int dmabuf_manage_release_secmem_session(void)
{
	mutex_lock(&g_no_head_mutex);
	if (g_secmem_inited) {
		if (g_shm_pool) {
			tee_shm_free(g_shm_pool);
			g_shm_pool = NULL;
		}

		if (g_secmem_context) {
			tee_client_close_session(g_secmem_context, g_secmem_session.session);
			tee_client_close_context(g_secmem_context);
			g_secmem_context = NULL;
		}
		g_secmem_inited = 0;
	}
	mutex_unlock(&g_no_head_mutex);
	return NOTIFY_OK;
}

static int dmabuf_manage_reboot_notifier(struct notifier_block *nb,
	unsigned long action, void *data)
{
	return dmabuf_manage_release_secmem_session();
}

static struct notifier_block dmabuf_manage_reboot_nb = {
	.notifier_call = dmabuf_manage_reboot_notifier,
};

#ifdef CONFIG_PM
static int dmabuf_manage_freeze_notifier(struct notifier_block *nb,
	unsigned long action, void *data)
{
	return dmabuf_manage_release_secmem_session();
}

static struct notifier_block dmabuf_manage_freeze_nb = {
	.notifier_call = dmabuf_manage_freeze_notifier,
};
#endif
#endif

int __init dmabuf_manage_init(void)
{
	int ret;

	ret = register_chrdev(0, DEVICE_NAME, &fops);
	if (ret < 0) {
		pr_error("register_chrdev failed\n");
		goto error_register;
	}
	dev_no = ret;
	ret = class_register(&dmabuf_manage_class);
	if (ret < 0) {
		pr_error("class_register failed\n");
		goto error_class;
	}
	dmabuf_manage_dev = device_create(&dmabuf_manage_class,
				   NULL, MKDEV(dev_no, 0),
				   NULL, DEVICE_NAME);
	if (IS_ERR(dmabuf_manage_dev)) {
		pr_error("device_create failed\n");
		ret = PTR_ERR(dmabuf_manage_dev);
		goto error_create;
	}
	REG_PATH_CONFIGS(CONFIG_PATH, dmabuf_manage_configs);
	INIT_LIST_HEAD(&pool_list);
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
#ifdef CONFIG_PM
	codec_mm_register_freeze_notifier(&dmabuf_manage_freeze_nb);
#endif
	register_reboot_notifier(&dmabuf_manage_reboot_nb);
#endif
	pr_dbg("init done\n");
	return 0;
error_create:
	class_unregister(&dmabuf_manage_class);
error_class:
	unregister_chrdev(dev_no, DEVICE_NAME);
error_register:
	return ret;
}

void __exit dmabuf_manage_exit(void)
{
	device_destroy(&dmabuf_manage_class, MKDEV(dev_no, 0));
	class_unregister(&dmabuf_manage_class);
	class_destroy(&dmabuf_manage_class);
	unregister_chrdev(dev_no, DEVICE_NAME);
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
#ifdef CONFIG_PM
	codec_mm_unregister_freeze_notifier(&dmabuf_manage_freeze_nb);
#endif
	unregister_reboot_notifier(&dmabuf_manage_reboot_nb);
#endif
	pr_dbg("exit done\n");
}

MODULE_LICENSE("GPL");
