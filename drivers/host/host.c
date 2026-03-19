// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/cma.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/kdebug.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <linux/mailbox_client.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/aml_mbox.h>
#include <dt-bindings/firmware/amlogic,firmware.h>
#include <linux/input.h>
#include "host.h"
#include "sysfs.h"
#include "host_poll.h"
#include "host_report.h"

#define SUSPEND_CLK_FREQ 24000000
static DEFINE_MUTEX(host_lock);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend host_early_suspend_handler;
#endif

#define HOST_NUM	2
static struct host_module *host_p[HOST_NUM];

bool host_firmware_ready(u8 host_id)
{
	if (host_id < HOST_NUM && host_p[host_id] &&
		host_p[host_id]->firmware_load && host_p[host_id]->firmware_started)
		return true;

	return false;
}
EXPORT_SYMBOL(host_firmware_ready);

struct device *host_to_device(u8 host_id)
{
	if (host_id < HOST_NUM && host_p[host_id])
		return host_p[host_id]->dev;

	return NULL;
}
EXPORT_SYMBOL(host_to_device);

int host_phyaddr_info(u8 host_id, phys_addr_t *addr, phys_addr_t *size)
{
	int ret = -1;

	if (host_id < HOST_NUM && host_p[host_id]) {
		switch (host_p[host_id]->start_pos) {
		case PURE_DDR:
			*addr = host_p[host_id]->phys_ddr_addr;
			*size = host_p[host_id]->phys_ddr_size;
			ret = 0;
			break;
		case PURE_SRAM:
			*addr = host_p[host_id]->phys_sram_addr;
			*size = host_p[host_id]->phys_sram_size;
			ret = 0;
			break;
		case DDR_SRAM:
			*addr = host_p[host_id]->phys_ddr_addr;
			*size = host_p[host_id]->phys_ddr_size;
			ret = 0;
			break;
		default:
			pr_err("%s host start position error!\n", __func__);
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(host_phyaddr_info);

/*free reserved memory*/
static unsigned long host_free_reserved_area(void *start, void *end, int poison, const char *s)
{
	void *pos;
	unsigned long pages = 0;

	start = (void *)PAGE_ALIGN((unsigned long)start);
	end = (void *)((unsigned long)end & PAGE_MASK);
	for (pos = start; pos < end; pos += PAGE_SIZE, pages++) {
		struct page *page = virt_to_page(pos);
		void *direct_map_addr;

		/*
		 * 'direct_map_addr' might be different from 'pos'
		 * because some architectures' virt_to_page()
		 * work with aliases.  Getting the direct map
		 * address ensures that we get a _writeable_
		 * alias for the memset().
		 */
		direct_map_addr = page_address(page);
		/*
		 * Perform a kasan-unchecked memset() since this memory
		 * has not been initialized.
		 */
		direct_map_addr = kasan_reset_tag(direct_map_addr);
		if ((unsigned int)poison <= 0xFF)
			memset(direct_map_addr, poison, PAGE_SIZE);

		free_reserved_page(page);
	}

	if (pages && s)
		pr_info("Freeing %s memory: %ldK\n",
			s, pages << (PAGE_SHIFT - 10));

	return pages;
}

static int host_dump_memory(const void *buf, unsigned int bytes, int col)
{
	int i = 0, n = 0, size = 0;
	const u8 *pdata;
	char str[256];
	char a_str[24];

	pdata = (u8 *)buf;
	size = bytes;
	memset(a_str, '\0', sizeof(a_str));
	memset(str, '\0', sizeof(str));

	while (n < size) {
		sprintf(a_str, "%p: ", pdata);
		strncat(str, a_str, sizeof(str) - 1);
		col = ((size - n) > col) ? col : (size - n);
		for (i = 0; i < col; i++) {
			sprintf(a_str, "%02x ", *(pdata + i));
			strncat(str, a_str, sizeof(str) - 1);
		}
		pr_debug("%s\n", str);
		memset(str, '\0', sizeof(str));/*re-init buf*/
		pdata += col;
		n += col;
	}

	return 0;
}

static unsigned int host_mbox_transfer(struct host_module *host)
{
	u32 cfg0;
	u32 addr;
	int ret;

	switch (host->start_pos) {
	case PURE_DDR:
		addr = host->phys_ddr_addr;
		break;
	case PURE_SRAM:
		addr = host->phys_sram_addr;
		break;
	default:
		return 0;
	};

	cfg0 = 0x1 |  1 << 1 | 1 << 2;
	host->host_dsp->mbox_buf.id = host->hostid;
	host->host_dsp->mbox_buf.addr = addr;
	host->host_dsp->mbox_buf.cfg0 = cfg0;

	ret = aml_mbox_transfer_data(host->init_mbox_chan,
				     MBOX_CMD_INIT_DSP,
				     &host->host_dsp->mbox_buf,
				     sizeof(host->host_dsp->mbox_buf),
				     NULL,
				     0,
				     MBOX_SYNC);
	if (ret < 0) {
		dev_err(host->dev, "mbox transfer data  error %d\n", ret);
		return ret;
	}

	return ret;
}

/*smc for bl40/m4/dsp*/
static unsigned long host_psci_smc(struct host_module *host, unsigned int smc_subid)
{
	struct arm_smccc_res res = {0};
	u32 addr;
	u32 id;
	u32 arg[5] = {0};

	switch (host->start_pos) {
	case PURE_DDR:
		addr = host->phys_ddr_addr;
		break;
	case PURE_SRAM:
		addr = host->phys_sram_addr;
		break;
	case DDR_SRAM:
		addr = host->phys_remap_addr;
		break;
	default:
		pr_err("%s host start position error!\n", __func__);
		return 0;
	};

	id = PACK_SMC_SUBID_ID(smc_subid, host->hostid);
	switch (smc_subid) {
	case SMC_SUBID_HIFI_DSP_BOOT:
		arg[0] = SMC_HIFI_DSP_CMD;
		arg[1] = id;
		arg[2] = addr;
		arg[3] = 0x1 |  1 << 1 | 1 << 2;
		arg[4] = 0;
		break;
	case SMC_SUBID_HIFI_DSP_REMAP:
		arg[0] = SMC_HIFI_DSP_CMD;
		arg[1] = id;
		arg[2] = addr;
		arg[3] = host->phys_sram_addr;
		arg[4] = 2;
		break;
	case SMC_SUBID_HIFI_DSP_RESET_HIGH:
		arg[0] = SMC_HIFI_DSP_CMD;
		arg[1] = id;
		arg[2] = 0;
		arg[3] = 0;
		arg[4] = 0;
		break;
	case SMC_SUBID_MFH_V1_BOOT:
		arg[0] = SMC_M4_CMD;
		arg[1] = id;
		arg[2] = addr;
		arg[3] = host->phys_ddr_size;
		arg[4] = 0;
		break;
	case SMC_SUBID_MFH_V2_BOOT:
		arg[0] = SMC_M4_CMD;
		arg[1] = id;
		arg[2] = addr;
		arg[3] = SMC_M4_BANK;
		arg[4] = 0;
		break;
	case SMC_SUBID_MFH_V2_RESET:
		arg[0] = SMC_M4_CMD;
		arg[1] = id;
		arg[2] = 0;
		arg[3] = 0;
		arg[4] = 0;
		break;
	default:
		pr_err("%s Invalid smc subid!\n", __func__);
		return 0;
	}

	arm_smccc_smc(arg[0], arg[1], arg[2], arg[3], arg[4], 0, 0, 0, &res);

	return res.a0;
}

static int host_clk_enable(struct host_module *host)
{
	int ret = 0;
	struct device *dev = host->dev;

	ret = clk_set_rate(host->clk, (unsigned long)host->clk_rate * 1000);
	if (ret) {
		dev_err(dev, "Can not set clock rate\n");
		return ret;
	}
	ret = clk_prepare_enable(host->clk);
	if (ret < 0) {
		dev_err(dev, "Can not enable clock\n");
		return ret;
	}

	return 0;
}

static void host_boot_prepare(struct host_module *host)
{
	if (!strstr(host->misc->name, "bl40"))
		host_clk_enable(host);
	if (strstr(host->misc->name, "bl40")) {
		aml_mbox_transfer_data(host->mbox_chan_to_dev,
				       MBOX_CMD_BL4_WAIT_UNLOCK,
				       NULL,
				       0,
				       NULL,
				       0,
				       MBOX_SYNC);
	}
	if (strstr(host->misc->name, "mfh"))
		host_psci_smc(host, SMC_SUBID_MFH_V2_RESET);
}

static void host_bootup(struct host_module *host)
{
	unsigned int smc_subid;

	if (!IS_ERR_OR_NULL(host->init_mbox_chan) && !IS_ERR_OR_NULL(host->host_dsp)) {
		host_mbox_transfer(host);
	} else {
		if (strstr(host->misc->name, "bl40"))
			smc_subid = SMC_SUBID_MFH_V1_BOOT;
		else if (strstr(host->misc->name, "mfh"))
			smc_subid = SMC_SUBID_MFH_V2_BOOT;
		else
			smc_subid = SMC_SUBID_HIFI_DSP_BOOT;

		if (smc_subid == SMC_SUBID_HIFI_DSP_BOOT && host->start_pos == DDR_SRAM)
			host_psci_smc(host, SMC_SUBID_HIFI_DSP_REMAP);

		host_psci_smc(host, smc_subid);
	}
}

static int host_fw_copy_to_memory(const struct firmware *fw,
				 const struct host_module *host,
				 void *fw_dst, const char *name)
{
	host_dump_memory(fw->data, 32, 16);
	host_dump_memory(fw->data + fw->size - 32, 32, 16);
	pr_debug("%s fw_src:0x%p, pdata_dst=0x%p, size=%zu bytes\n",
			__func__, fw->data, fw_dst, fw->size);
	memcpy_toio(fw_dst, fw->data, fw->size);
	/*cache clean*/
	if (!strstr(name, "sram"))
		dma_sync_single_for_device(host->dev,
					   host->phys_ddr_addr,
					   host->phys_ddr_size,
					   DMA_TO_DEVICE);
	pr_debug("\n after copy to ddr and clean cache:\n");
	host_dump_memory(fw_dst, 32, 16);
	host_dump_memory(fw_dst + fw->size - 32, 32, 16);

	return 0;
}

static int firmware_load(struct host_module *host, void *fw_dst, const char *name)
{
	const struct firmware *fw = NULL;
	int err = 0;

	if (IS_ERR_OR_NULL(host))
		return -EINVAL;

	err = request_firmware(&fw, name, host->dev);
	if (err < 0) {
		pr_err("can't load the %s,err=%d\n", name, err);
		return err;
	}

	if (strstr(host->host_data->name, "m4"))
		host->phys_ddr_size = fw->size;
	host_fw_copy_to_memory(fw, host, fw_dst, name);
	release_firmware(fw);
	return 0;
}

static int host_firmware_load(struct host_module *host)
{
	int err = -EINVAL;
	u8 start_pos = host->start_pos;

	switch (start_pos) {
	case PURE_DDR:
		err = firmware_load(host, host->ddr_addr, host->fname0);
		break;
	case PURE_SRAM:
		err = firmware_load(host, host->sram_addr, host->fname1);
		break;
	case DDR_SRAM:
		err = firmware_load(host, host->ddr_addr, host->fname0);
		if (err)
			return err;
		err = firmware_load(host, host->sram_addr, host->fname1);
		if (err)
			return err;
		break;
	default:
		break;
	};
	return err;
}

static void *mm_vmap(phys_addr_t phys, unsigned long size, pgprot_t pgprotattr)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot = pgprotattr;
	void *vaddr;
	int i;

	offset = offset_in_page(phys);
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
		       npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	pr_debug("[HIGH-MEM-MAP] pa(%lx) to va(%p), size: %d\n",
		 (unsigned long)phys, vaddr, npages << PAGE_SHIFT);

	return vaddr;
}

void host_core_reset(struct host_module *host)
{
	if (host->core_reset) {
		pr_info("[%s][%s will be reset!!!!]\n", __func__, host->host_data->name);
		host_psci_smc(host, SMC_SUBID_HIFI_DSP_RESET_HIGH);
		pm_runtime_put_sync(host->dev);
		writel(0, host->health_reg);
		if (host->start_pos == PURE_SRAM) {
			memset_io((void *)host->sram_addr, '\0', (host->phys_sram_size));
		} else if (host->start_pos == DDR_SRAM) {
			memset((void *)host->ddr_addr, '\0', (host->phys_ddr_size));
			memset_io((void *)host->sram_addr, '\0', (host->phys_sram_size));
		} else {
			memset((void *)host->ddr_addr, '\0', (host->phys_ddr_size));
		}
		host->firmware_load = 0;
		pm_runtime_get_sync(host->dev);
	} else {
		pr_err("core reset is not supported\n");
	}
}

static int __maybe_unused host_runtime_suspend(struct device *dev)
{
	struct host_module *host = dev_get_drvdata(dev);
	char message[] = "MBOX_CMD_HIFI4STOP";

	if (IS_ERR_OR_NULL(host))
		return -EINVAL;

	if (strstr(host->misc->name, "mfh"))
		host_psci_smc(host, SMC_SUBID_MFH_V2_RESET);
	else if (strstr(host->misc->name, "hifi4dsp")) {
		if (!host->hang) {
			aml_mbox_transfer_data(host->mbox_chan_to_dev,
					       MBOX_CMD_HIFI4STOP,
					       message,
					       sizeof(message),
					       NULL,
					       0,
					       MBOX_SYNC);
			msleep(50);
		}
	} else {
		return 0;
	}

	clk_disable_unprepare(host->clk);
	host_health_monitor_stop(host);
	host_logbuff_stop(host);
	if (strstr(host->misc->name, "hifi4dsp") && host->host_dsp->pm_support_ffv)
		host_dsp_ffv_wq_stop(host->host_dsp);

	pr_info("[%s %d]host stop success!\n", __func__, __LINE__);
	return 0;
}

static int __maybe_unused host_runtime_resume(struct device *dev)
{
	struct host_module *host = dev_get_drvdata(dev);
	int ret;

	if (!host->firmware_load) {
		ret = host_firmware_load(host);
		if (ret) {
			dev_err(dev, "Load firmware fail\n");
			return ret;
		}
	}

	host_boot_prepare(host);
	host_bootup(host);
	host_health_monitor_start(host);
	host_logbuff_start(host);

	return 0;
}

static void host_mbox_callback_from_dev(struct mbox_client *cl, void *msg)
{
	struct host_module *host = dev_get_drvdata(cl->dev);
	struct aml_mbox_rx_data *mbox_msg = msg;
	u32 mbox_cmd = mbox_msg->cmd;
	u32 mbox_data = *(u32 *)(mbox_msg->buf);

	pr_debug("host receive cmd 0x%x, data 0x%x\n", mbox_cmd, mbox_data);

	if (mbox_cmd == MBX_CMD_VAD_AWE_WAKEUP &&
	    mbox_data == DSP_VAD_WAKUP_ARM) {
		pr_info("input event: vad wakeup in early suspend\n");
		host_dsp_vad_report(host->host_dsp);
	}
}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void host_early_suspend(struct early_suspend *h)
{
	struct host_module *host = h->param;
	char message[30];
	unsigned long clk_rate;

	if (pm_runtime_suspended(host->dev))
		return;

	if (pm_runtime_active(host->dev) && host->host_dsp->pm_support_suspend) {
		pr_debug("early suspend: AP send suspend cmd to dsp...\n");
		strncpy(message, "HIFI_EARLY_SUSPEND_WITH_FFV", sizeof(message));
		aml_mbox_transfer_data(host->mbox_chan_to_dev,
				       MBOX_CMD_HIFI4SUSPEND,
				       message,
				       sizeof(message),
				       message,
				       sizeof(message),
				       MBOX_SYNC);

		if (!IS_ERR_OR_NULL(host->host_dsp->clk_hifi))
			clk_rate = clk_get_rate(host->host_dsp->clk_hifi) / 2;
		else
			clk_rate = SUSPEND_CLK_FREQ;

		if (clk_set_rate(host->clk, clk_rate) == 0) {
			pr_info("early suspend: switch dsp clk to %ld Hz\n", clk_rate);
		} else {
			pr_info("early suspend: switch dsp clk to 24 MHz\n");
			clk_set_rate(host->clk, SUSPEND_CLK_FREQ);
		}
	}
}

static void host_late_resume(struct early_suspend *h)
{
	struct host_module *host = h->param;
	char message[30];

	if (pm_runtime_suspended(host->dev))
		return;

	if (pm_runtime_active(host->dev) && host->host_dsp->pm_support_suspend) {
		if (clk_set_rate(host->clk, (unsigned long)host->clk_rate * 1000) == 0)
			pr_info("late resume: switch dsp clk to %d Hz\n", host->clk_rate * 1000);

		pr_debug("late resume: AP send resume cmd to dsp...\n");
		strncpy(message, "HIFI_LATE_RESUME_WITH_FFV", sizeof(message));
		aml_mbox_transfer_data(host->mbox_chan_to_dev,
				       MBOX_CMD_HIFI4RESUME,
				       message,
				       sizeof(message),
				       message,
				       sizeof(message),
				       MBOX_SYNC);
	}
}

static void register_host_early_suspend_handler(struct host_module *host)
{
	host_early_suspend_handler.suspend = host_early_suspend;
	host_early_suspend_handler.resume = host_late_resume;
	host_early_suspend_handler.param = host;
	register_early_suspend(&host_early_suspend_handler);
}
#endif

static int host_suspend(struct device *dev)
{
	struct host_module *host = dev_get_drvdata(dev);
	char message[30];

	if (pm_runtime_suspended(dev))
		return 0;

	if (pm_runtime_active(dev) && host->host_dsp->pm_support_suspend) {
		if (host->host_dsp->pm_support_ffv)
			return 0;

		pr_debug("AP send suspend cmd to dsp...\n");
		strncpy(message, "HIFI_DEEP_SLEEP", sizeof(message));
		aml_mbox_transfer_data(host->mbox_chan_to_dev,
				       MBOX_CMD_HIFI4SUSPEND,
				       message,
				       sizeof(message),
				       message,
				       sizeof(message),
				       MBOX_SYNC);

		/*clk = 24 M*/
		clk_set_rate(host->clk, SUSPEND_CLK_FREQ);
	} else if (!host->host_dsp->pm_support_always_on)
		clk_disable_unprepare(host->clk);

	return 0;
}

static int host_resume(struct device *dev)
{
	struct host_module *host = dev_get_drvdata(dev);
	char message[30];
	int ret = 0;

	if (pm_runtime_suspended(dev))
		return 0;

	if (pm_runtime_active(dev) && host->host_dsp->pm_support_suspend) {
		if (host->host_dsp->pm_support_ffv) {
			if (get_resume_method() == VAD_WAKEUP) {
				pr_info("input event: vad wakeup in deep sleep\n");
				host_dsp_vad_report(host->host_dsp);
			}

			return 0;
		}

		/*clk = Max M*/
		clk_set_rate(host->clk, (unsigned long)host->clk_rate * 1000);

		pr_debug("AP send resume cmd to dsp...\n");
		strncpy(message, "HIFI_RESUME", sizeof(message));
		aml_mbox_transfer_data(host->mbox_chan_to_dev,
				       MBOX_CMD_HIFI4RESUME,
				       message,
				       sizeof(message),
				       message,
				       sizeof(message),
				       MBOX_SYNC);
	} else if (!host->host_dsp->pm_support_always_on) {
		ret = clk_prepare_enable(host->clk);
		if (ret < 0) {
			dev_err(dev, "Can not enable clock\n");
			return ret;
		}
	}

	return 0;
}

static int host_miscdev_open(struct inode *inode, struct file *fp)
{
	struct miscdevice *misc = fp->private_data;
	struct host_data *host_data = container_of(misc, struct host_data, misc);

	fp->private_data = host_data->host;

	return 0;
}

static int host_miscdev_release(struct inode *inode, struct file *fp)
{
	return 0;
}

static long host_miscdev_unlocked_ioctl(struct file *fp, unsigned int cmd,
					    unsigned long arg)
{
	struct host_module *host;
	struct device *dev;
	void __user *argp;
	int ret = 0;

	mutex_lock(&host_lock);
	if (!fp->private_data) {
		pr_debug("%s error:fp->private_data is null", __func__);
		ret = -1;
		mutex_unlock(&host_lock);
		return ret;
	}
	argp = (void __user *)arg;
	host = fp->private_data;
	dev = host->dev;

	if (strstr(host->host_data->name, "dsp")) {
		host->host_dsp->usrinfo = kmalloc(sizeof(*host->host_dsp->usrinfo), GFP_KERNEL);
		if (!host->host_dsp->usrinfo) {
			ret = -1;
			goto err;
		}
	}

	switch (cmd) {
	case HOST_LOAD:
		pr_debug("%s HOST_LOAD\n", __func__);
		ret = copy_from_user(host->host_dsp->usrinfo, argp,
				     sizeof(struct dsp_info_t));
		if (ret) {
			pr_err("%s error: HOST_LOAD is error", __func__);
			goto err;
		}
		host->host_dsp->usrinfo->fw_name[31] = '\0';
		if (host->start_pos == PURE_SRAM)
			strcpy(host->fname1, host->host_dsp->usrinfo->fw_name);
		else
			strcpy(host->fname0, host->host_dsp->usrinfo->fw_name);
		host_firmware_load(host);
		host->firmware_load = 1;
	break;
	case HOST_2LOAD:
		pr_debug("%s HOST_2LOAD\n", __func__);
		ret = copy_from_user(host->host_dsp->usrinfo, argp,
				     sizeof(struct dsp_info_t));
		if (ret) {
			pr_err("%s error: HOST_2LOAD is error", __func__);
			goto err;
		}
		host->host_dsp->usrinfo->fw1_name[31] = '\0';
		host->host_dsp->usrinfo->fw2_name[31] = '\0';
		strcpy(host->fname0, host->host_dsp->usrinfo->fw1_name);
		strcpy(host->fname1, host->host_dsp->usrinfo->fw2_name);
		host_firmware_load(host);
		host->firmware_load = 1;
	break;
	case HOST_START:
		pr_debug("%s HOST_START\n", __func__);
		ret = copy_from_user(host->host_dsp->usrinfo, argp,
				     sizeof(struct dsp_info_t));
		if (ret) {
			pr_err("%s error: HOST_START is error", __func__);
			goto err;
		}
		pm_runtime_get_sync(dev);
		host->firmware_started = 1;
	break;
	case HOST_STOP:
		pr_debug("%s HOST_STOP\n", __func__);
		ret = copy_from_user(host->host_dsp->usrinfo, argp,
				     sizeof(struct dsp_info_t));
		if (ret) {
			pr_err("%s error: HOST_STOP is error", __func__);
			goto err;
		}
		pm_runtime_put_sync(dev);
		host->firmware_started = 0;
	break;
	case HOST_GET_INFO:
		pr_debug("%s HOST_GET_INFO\n", __func__);
		ret = copy_from_user(host->host_dsp->usrinfo, argp,
				     sizeof(struct dsp_info_t));
		if (ret) {
			pr_err("%s error: HOST_GET_INFO copy_from_user is error", __func__);
			goto err;
		}
		strncpy(host->host_dsp->usrinfo->fw_name, host->host_data->name,
						sizeof(host->host_dsp->usrinfo->fw_name));
		host->host_dsp->usrinfo->phy_addr = host->phys_ddr_addr;
		host->host_dsp->usrinfo->size = host->phys_ddr_size;
		ret = copy_to_user(argp, host->host_dsp->usrinfo,
				   sizeof(struct dsp_info_t));
		if (ret) {
			pr_err("%s error: HOST_GET_INFO copy_to_user is error", __func__);
			goto err;
		}
		pr_debug("%s HOST_GET_INFO %s\n", __func__,
			 host->host_dsp->usrinfo->fw_name);
	break;
	case HOST_SHM_CLEAN:
		ret = copy_from_user(&host->host_dsp->shminfo, argp,
						sizeof(host->host_dsp->shminfo));
		pr_debug("%s clean cache, addr:%u, size:%u\n",
			 __func__, host->host_dsp->shminfo.addr, host->host_dsp->shminfo.size);
		if (ret || host->host_dsp->shminfo.addr > ((host->phys_ddr_addr +
					host->phys_ddr_size) - host->phys_shm_size) ||
					host->host_dsp->shminfo.addr < host->phys_ddr_addr ||
					host->host_dsp->shminfo.size > ((host->phys_ddr_addr +
					host->phys_ddr_size) - host->phys_shm_size -
							host->host_dsp->shminfo.addr)) {
			pr_err("%s error: HIFI4DSP_SHM_CLEAN is error", __func__);
			goto err;
		}
		dma_sync_single_for_device(host->dev,
					   host->host_dsp->shminfo.addr,
					   host->host_dsp->shminfo.size,
					   DMA_TO_DEVICE);
	break;
	case HOST_SHM_INV:
		ret = copy_from_user(&host->host_dsp->shminfo, argp,
						sizeof(host->host_dsp->shminfo));
		pr_debug("%s invalidate cache, addr:%u, size:%u\n",
			 __func__, host->host_dsp->shminfo.addr, host->host_dsp->shminfo.size);
		if (ret || host->host_dsp->shminfo.addr > ((host->phys_ddr_addr +
					host->phys_ddr_size) - host->phys_shm_size) ||
					host->host_dsp->shminfo.addr < host->phys_ddr_addr ||
					host->host_dsp->shminfo.size > ((host->phys_ddr_addr +
					host->phys_ddr_size) - host->phys_shm_size -
							host->host_dsp->shminfo.addr)) {
			pr_err("%s error: HIFI4DSP_SHM_INV is error", __func__);
			goto err;
		}
		dma_sync_single_for_device(host->dev,
					   host->host_dsp->shminfo.addr,
					   host->host_dsp->shminfo.size,
					   DMA_FROM_DEVICE);
	break;
	case MFH_FIRMWARE_LOAD:
		ret = copy_from_user((void *)&host->host_mfh->mfh_info,
				     argp, sizeof(host->host_mfh->mfh_info));
		if (ret < 0)
			goto err;
		host->host_mfh->mfh_info.name[29] = '\0';
		strcpy(host->fname0, host->host_mfh->mfh_info.name);
		host_runtime_resume(dev);
	break;
	default:
		pr_err("%s ioctl CMD error\n", __func__);
	break;
	}
err:
	if (strstr(host->host_data->name, "dsp")) {
		kfree(host->host_dsp->usrinfo);
		host->host_dsp->usrinfo = NULL;
	}
	mutex_unlock(&host_lock);

	return ret;
}

#ifdef CONFIG_COMPAT
static long host_miscdev_compat_ioctl(struct file *filp,
					  unsigned int cmd, unsigned long args)
{
	long ret;

	args = (unsigned long)compat_ptr(args);
	ret = host_miscdev_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif

/*shm mmap*/
static int host_miscdev_mmap(struct file *fp, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long phys_page_addr = 0;
	unsigned long size = 0;
	struct host_module *host;

	if (!vma) {
		pr_err("input error: vma is NULL\n");
		return -1;
	}

	if (!fp->private_data) {
		pr_err("%s error:fp->private_data is null", __func__);
		return -1;
	}

	host = fp->private_data;

	phys_page_addr = host->phys_ddr_addr >> PAGE_SHIFT;
	size = ((unsigned long)vma->vm_end - (unsigned long)vma->vm_start);
	pr_debug("vma=0x%pK.\n", vma);
	pr_debug("size=%ld, vma->vm_start=%ld, end=%ld.\n",
	       ((unsigned long)vma->vm_end - (unsigned long)vma->vm_start),
	       (unsigned long)vma->vm_start, (unsigned long)vma->vm_end);
	pr_debug("phys_page_addr=%ld.\n", (unsigned long)phys_page_addr);

	vma->vm_page_prot = PAGE_SHARED;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (size > host->phys_ddr_size)
		size = host->phys_ddr_size;

	ret = remap_pfn_range(vma,
			      vma->vm_start,
			      phys_page_addr, size, vma->vm_page_prot);
	if (ret != 0) {
		pr_err("remap_pfn_range ret=%d\n", ret);
		return -1;
	}

	return ret;
}

static const struct dev_pm_ops host_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(host_suspend,
	host_resume)
	SET_RUNTIME_PM_OPS(host_runtime_suspend,
			   host_runtime_resume, NULL)
};

static const struct file_operations host_miscdev_fops = {
	.owner = THIS_MODULE,
	.open = host_miscdev_open,
	.read = NULL,
	.write = NULL,
	.release = host_miscdev_release,
	.unlocked_ioctl = host_miscdev_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = host_miscdev_compat_ioctl,
#endif
	.mmap = host_miscdev_mmap,
};

static struct host_data host_data_table[] = {
	{
		.name = "dspboota",
		.hostid = 0,
		.misc = {
			.minor  = MISC_DYNAMIC_MINOR,
			.name   = "hifi4dsp0",
			.fops   = &host_miscdev_fops,
		},
	},
	{
		.name = "dspbootb",
		.hostid = 1,
		.misc = {
			.minor  = MISC_DYNAMIC_MINOR,
			.name   = "hifi4dsp1",
			.fops   = &host_miscdev_fops,

		},
	},
	{
		.name = "m4a",
		.hostid = 0,
		.misc = {
			.minor  = MISC_DYNAMIC_MINOR,
			.name   = "bl40",
			.fops   = &host_miscdev_fops,
		},
	},
	{
		.name = "m4a",
		.hostid = 0,
		.misc = {
			.minor  = MISC_DYNAMIC_MINOR,
			.name   = "mfh0",
			.fops   = &host_miscdev_fops,
		},
	},
	{
		.name = "m4b",
		.hostid = 1,
		.misc = {
			.minor  = MISC_DYNAMIC_MINOR,
			.name   = "mfh1",
			.fops   = &host_miscdev_fops,
		},
	}
};

static const struct of_device_id host_device_id[] = {
	{
		.compatible = "amlogic, hifidsp0",
		.data = &host_data_table[0],
	},
	{
		.compatible = "amlogic, hifidsp1",
		.data = &host_data_table[1],
	},
	{
		.compatible = "amlogic, bl40",
		.data = &host_data_table[2],
	},
	{
		.compatible = "amlogic, mfh0",
		.data = &host_data_table[3],
	},
	{
		.compatible = "amlogic, mfh1",
		.data = &host_data_table[4],
	},
	{},
};
MODULE_DEVICE_TABLE(of, host_device_id);

void host_parse_firmware_name(struct host_module *host, struct host_data *host_data)
{
	switch (host->start_pos) {
	case PURE_DDR:
		snprintf(host->fname0, HOSTFW_NAME_LEN - 1,
			 "%s_ddr.bin", host_data->name);
		break;
	case PURE_SRAM:
		snprintf(host->fname1, HOSTFW_NAME_LEN - 1,
			 "%s_sram.bin", host_data->name);
		break;
	case DDR_SRAM:
		snprintf(host->fname0, HOSTFW_NAME_LEN - 1,
			 "%s_ddr.bin", host_data->name);
		snprintf(host->fname1, HOSTFW_NAME_LEN - 1,
			 "%s_sram.bin", host_data->name);
		break;
	};
}

static void host_platform_remove(struct platform_device *pdev)
{
	struct host_module *host = platform_get_drvdata(pdev);

	host_destroy_debugfs_files(host);
	host_destroy_device_files(&pdev->dev);
}

static int host_firmware_reserved_ddr(struct platform_device *pdev, struct reserved_mem *fwmem,
						struct host_module *host)
{
	int ret = -1;
	struct device_node *mem_node;
	struct resource res = {0};

	mem_node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!mem_node)
		goto out;
	ret = of_address_to_resource(mem_node, 0, &res);
	if (ret) {
		dev_err(&pdev->dev, "resource memory init failed\n");
		goto out;
	}

	of_node_put(mem_node);
	fwmem->base = res.start;
out:
	return ret;
}

static int host_parse_devtree(struct platform_device *pdev, struct host_module *host)
{
	struct resource *resource;
	const char *clk_name = NULL;
	struct reserved_mem fwmem = {0};
	struct device *dev = &pdev->dev;
	struct mbox_chan *mbox_chan;
	int ret;

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "device-support-reg");
	if (!IS_ERR_OR_NULL(resource)) {
		host->device_spt_reg = devm_ioremap(dev,
						    resource->start,
						    resource->end - resource->start + 1);
		if (IS_ERR_OR_NULL(host->device_spt_reg))
			dev_err(&pdev->dev, "failed to ioremap device support register\n");
	}

	if ((!IS_ERR_OR_NULL(host->device_spt_reg) &&
			    (readl(host->device_spt_reg) & host->device_support_bit))) {
		dev_err(&pdev->dev, "this device not support %s\n", host->misc->name);
		return -EINVAL;
	}

	if (!strstr(host->misc->name, "bl40")) {
		host->base_reg = devm_platform_ioremap_resource_byname(pdev, "base-reg");
		if (IS_ERR_OR_NULL(host->base_reg)) {
			dev_err(dev, "Reg base register is error\n");
			return -EINVAL;
		}
	}

	host->health_reg = devm_platform_ioremap_resource_byname(pdev, "health-reg");
	if (IS_ERR_OR_NULL(host->health_reg))
		dev_err(dev, "Not support health monitor\n");

	if (!host_firmware_reserved_ddr(pdev, &fwmem, host)) {
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ddrfw-region");
		if (!IS_ERR_OR_NULL(resource)) {
			host->phys_ddr_size = resource->end - resource->start + 1;
			host->phys_ddr_addr = fwmem.base + resource->start;
			host->ddr_addr = mm_vmap(host->phys_ddr_addr,
							host->phys_ddr_size,
							pgprot_dmacoherent(PAGE_KERNEL));
		}

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "shm-region");
		if (!IS_ERR_OR_NULL(resource)) {
			host->phys_shm_size = resource->end - resource->start + 1;
			host->phys_shm_addr = fwmem.base + resource->start;
			host->shm_addr = mm_vmap(host->phys_shm_addr,
							host->phys_shm_size,
							pgprot_dmacoherent(PAGE_KERNEL));
		}

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "remap-region");
		if (!IS_ERR_OR_NULL(resource)) {
			host->phys_remap_size = resource->end - resource->start + 1;
			host->phys_remap_addr = resource->start;
		}
	}

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram-region");
	if (!IS_ERR_OR_NULL(resource)) {
		host->phys_sram_size = resource->end - resource->start + 1;
		host->phys_sram_addr = resource->start;
		host->sram_addr = devm_ioremap(dev, resource->start,
							host->phys_sram_size);
	}

	if (of_property_read_u8(dev->of_node, "pm-support", &host->pm_support))
		dev_err(dev, "Not find pm-support\n");

	if (host->pm_support) {
		host->host_dsp->pm_support_suspend = PM_SUPPORT_DSP_SUSPEND(host->pm_support);
		host->host_dsp->pm_support_always_on = PM_SUPPORT_DSP_ALWAYS_ON(host->pm_support);
		host->host_dsp->pm_support_ffv = PM_SUPPORT_DSP_FFV(host->pm_support);
	}

	if (!of_property_read_string_index(dev->of_node, "clock-names", 0, &clk_name)) {
		host->clk = devm_clk_get(dev, clk_name);
		if (IS_ERR_OR_NULL(host->clk)) {
			dev_err(dev, "can't get clk\n");
			goto err;
		} else {
			ret = of_property_read_u32(dev->of_node, "clkfreq-khz", &host->clk_rate);
			if (ret) {
				dev_err(&pdev->dev, "of get clkfreq-khz failed\n");
				goto err;
			}
		}
	}

	if (strstr(host->misc->name, "hifi4dsp")) {
		if (host->host_dsp->pm_support_ffv &&
		    !of_property_read_string_index(dev->of_node, "clock-names", 1, &clk_name)) {
			host->host_dsp->clk_hifi = devm_clk_get(dev, clk_name);
			if (IS_ERR_OR_NULL(host->host_dsp->clk_hifi))
				dev_warn(dev, "can't get clk_hifi\n");
		}
	}

	/* mbox channel request */
	mbox_chan = aml_mbox_request_channel_byname(&pdev->dev, "init_dsp");
	if (!IS_ERR_OR_NULL(mbox_chan))
		host->init_mbox_chan = mbox_chan;

	host->mbox_chan_to_dev = aml_mbox_request_channel_byidx(&pdev->dev, 0);
	if (IS_ERR_OR_NULL(host->mbox_chan_to_dev))
		dev_err(dev, "Not find DSP mailbox channel\n");

	host->mbox_chan_from_dev = aml_mbox_request_channel_byidx(&pdev->dev, 1);
	if (IS_ERR_OR_NULL(host->mbox_chan_from_dev))
		dev_err(dev, "Not find mailbox channel from dev\n");
	else
		host->mbox_chan_from_dev->cl->rx_callback = host_mbox_callback_from_dev;
	host->core_reset = of_property_read_bool(dev->of_node, "core-reset");
	of_property_read_u32(dev->of_node, "logbuff-polling-ms",
			&host->logbuff_polling_ms);
	of_property_read_u32(dev->of_node, "health-polling-ms",
			&host->health_polling_ms);

	ret = of_property_read_u8(dev->of_node, "startup-position", &host->start_pos);
	if (ret) {
		dev_err(dev, "Not find startup-position\n");
		goto err;
	}

	return 0;

err:
	return host_free_reserved_area(__va(fwmem.base), __va(PAGE_ALIGN(fwmem.base +
				host->phys_ddr_size)), 0, "free_reserved");
}

static int host_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct host_module *host;
	struct host_dsp *host_dsp = NULL;
	struct host_mfh *host_mfh = NULL;
	struct host_data *host_data;
	int ret;

	host_data = (struct host_data *)of_device_get_match_data(dev);
	if (IS_ERR_OR_NULL(host_data)) {
		dev_err(&pdev->dev, "failed to get host data\n");
		return -ENODEV;
	}

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (IS_ERR_OR_NULL(host))
		return -ENOMEM;

	if (strstr(host_data->name, "m4")) {
		host_mfh = devm_kzalloc(dev, sizeof(*host_mfh), GFP_KERNEL);
		if (IS_ERR_OR_NULL(host))
			return -ENOMEM;

		host->host_mfh = host_mfh;
		if (of_property_read_u32(dev->of_node, "m4-support-bit", &host->device_support_bit))
			dev_err(dev, "Not find m4-support-bit\n");
	}

	if (strstr(host_data->name, "dsp")) {
		host_dsp = devm_kzalloc(dev, sizeof(*host_dsp), GFP_KERNEL);
		if (IS_ERR_OR_NULL(host))
			return -ENOMEM;

		host->host_dsp = host_dsp;
		host->host_dsp->host = host;
		if (of_property_read_u32(dev->of_node, "dsp-support-bit",
					 &host->device_support_bit))
			dev_err(dev, "Not find dsp-support-bit\n");
	}

	host->dev = dev;
	host->hostid = host_data->hostid;
	host->misc = &host_data->misc;
	host->host_data = host_data;
	ret = host_parse_devtree(pdev, host);
	if (ret)
		return ret;
	host_parse_firmware_name(host, host_data);
	ret = misc_register(&host_data->misc);
	if (ret) {
		dev_err(dev, "Misc register fail\n");
		return ret;
	}

	pm_runtime_enable(dev);
	host_create_debugfs_files(host);
	host_create_device_files(&pdev->dev);
	device_init_wakeup(&pdev->dev, 1);
	host_data->host = host;
	platform_set_drvdata(pdev, host);

	if (strstr(host_data->name, "dsp") && host->host_dsp->pm_support_ffv) {
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
		register_host_early_suspend_handler(host);
#endif
		host->host_dsp->input_device = input_allocate_device();
		if (!host->host_dsp->input_device)
			return -ENOMEM;

		host_dsp_vad_input_device_init(host->host_dsp);
		if (input_register_device(host->host_dsp->input_device)) {
			input_free_device(host->host_dsp->input_device);
			return -EINVAL;
		}

		host_dsp_ffv_wq_init(host->host_dsp);
	}

	host_p[host->hostid] = host;

	return 0;
}

static void host_shutdown(struct platform_device *pdev)
{
	pm_runtime_force_suspend(&pdev->dev);
}

static struct platform_driver host_platform_driver = {
	.driver = {
		.name  = "amlogic_host",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(host_device_id),
		.pm = &host_pm_ops,
	},
	.probe  = host_platform_probe,
	.remove = host_platform_remove,
	.shutdown = host_shutdown,
};
module_platform_driver(host_platform_driver);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("Host Module Driver");
MODULE_LICENSE("GPL v2");

