// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/extcon-provider.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>

#include <drm/drmP.h>

#include <drm/amlogic/meson_connector_dev.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_types.h>

#include "hdmitx_hdcp_type.h"
#include "hdmitx_ddc.h"
#include "hdmitx_hw.h"
#include "hdmitx_hdcp.h"

static void drm_hdmitx_disable_hdcp_mode(unsigned int content_type);
static void drm_hdmitx_enable_hdcp_mode(unsigned int content_type);
static void drm_tx_hdcp22_init(struct hdmitx_common *tx_comm);

static DEFINE_MUTEX(mutex);
/* obs_cur, record current obs */
static struct hdcp_obs_val obs_cur;
/* if obs_cur is differ than last time, then save to obs_last */
static struct hdcp_obs_val obs_last;
static int write_buff(const char *fmt, ...);

static struct hdcplog_buf hdcplog_buf;

/* verify ksv, 20 ones and 20 zeroes */
int hdcp_ksv_valid(unsigned char *dat)
{
	int i, j, one_num = 0;

	for (i = 0; i < 5; i++) {
		for (j = 0; j < 8; j++) {
			if ((dat[i] >> j) & 0x1)
				one_num++;
		}
	}
	return one_num == 20;
}

static int save_obs_val(struct hdmitx_hw_common *tx_hw, struct hdcp_obs_val *obs)
{
	return hdmitx_hw_cntl(tx_hw, HDCP14_SAVE_OBS, (void *)obs, NULL);
}

static void pr_obs(struct hdcp_obs_val *obst0, struct hdcp_obs_val *obst1,
	unsigned int mask)
{
	/* if idx > HDCP_LOG_SIZE, then set idx as 0 for wrapping log */
#define GETBITS(val, start, len) (((val) >> (start)) & ((1 << (len)) - 1))
#define DIFF_ST(_v1, v0, _s, _l, str) \
	do { \
		typeof(_v1) v1 = (_v1); \
		typeof(_s) s = (_s); \
		typeof(_l) l = (_l); \
		if (GETBITS(v1, s, l) != (GETBITS(v0, s, l))) { \
			write_buff("%s: %x\n", str, GETBITS(v1, s, l)); \
		} \
	} while (0)

	if (mask & (1 << 0)) {
		if (GETBITS(obst1->obs0, 1, 7) != GETBITS(obst0->obs0, 1, 7)) {
			write_buff("StateA: %x  SubStateA: %x\n",
				GETBITS(obst1->obs0, 4, 4),
				GETBITS(obst1->obs0, 1, 3));
		}
		DIFF_ST(obst1->obs0, obst0->obs0, 0, 1, "hdcpengaged");
	}
	if (mask & (1 << 1)) {
		DIFF_ST(obst1->obs1, obst0->obs1, 4, 3, "StateOEG");
		DIFF_ST(obst1->obs1, obst0->obs1, 0, 4, "StateR");
	}
	if (mask & (1 << 2)) {
		DIFF_ST(obst1->obs2, obst0->obs2, 3, 3, "StateE");
		DIFF_ST(obst1->obs2, obst0->obs2, 0, 3, "StateEEG");
	}
	if (mask & (1 << 3)) {
		DIFF_ST(obst1->obs3, obst0->obs3, 7, 1, "RSVD");
		DIFF_ST(obst1->obs3, obst0->obs3, 6, 1, "Repeater");
		DIFF_ST(obst1->obs3, obst0->obs3, 5, 1, "KSV_FIFO_Ready");
		DIFF_ST(obst1->obs3, obst0->obs3, 4, 1, "Fast_i2c");
		DIFF_ST(obst1->obs3, obst0->obs3, 3, 1, "RSVD2");
		DIFF_ST(obst1->obs3, obst0->obs3, 2, 1, "Hdmi_mode");
		DIFF_ST(obst1->obs3, obst0->obs3, 1, 1, "Features1.1");
		DIFF_ST(obst1->obs3, obst0->obs3, 0, 1, "Fast_Reauth");
	}
	if (mask & (1 << 4)) {
		DIFF_ST(obst1->intstat, obst0->intstat, 7, 1, "hdcp_engaged");
		DIFF_ST(obst1->intstat, obst0->intstat, 6, 1, "hdcp_failed");
		DIFF_ST(obst1->intstat, obst0->intstat, 4, 1, "i2cnack");
		DIFF_ST(obst1->intstat, obst0->intstat, 3, 1,
			"lost_arbitration");
		DIFF_ST(obst1->intstat, obst0->intstat, 2, 1,
			"keep_out_error_int");
		DIFF_ST(obst1->intstat, obst0->intstat, 1, 1, "KSV_sha1_calc_int");
	}
}

static void _hdcp_do_work(struct work_struct *work)
{
	int ret = 0;
	bool arg;
	struct hdmitx20_dev *hdev =
		container_of(work, struct hdmitx20_dev, work_do_hdcp.work);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;

	switch (tx_comm->hdcptx_comm.hdcp_mode) {
	case 2:
		/* arg = true; */
		/* hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_CLKDIS, (void *)&arg, NULL); */
		/* schedule_delayed_work(&hdev->work_do_hdcp, 5); */
		break;
	case 1:
		mutex_lock(&mutex);
		ret = save_obs_val(tx_comm->tx_hw, &obs_cur);
		/* ret is NZ, then update obs_last */
		if (ret) {
			pr_obs(&obs_last, &obs_cur, ret);
			obs_last = obs_cur;
		}
		mutex_unlock(&mutex);
		/* log time frequency */
		schedule_delayed_work(&hdev->work_do_hdcp, msecs_to_jiffies(50));
		break;
	case 0:
	default:
		mutex_lock(&mutex);
		memset(&obs_cur, 0, sizeof(obs_cur));
		memset(&obs_last, 0, sizeof(obs_last));
		mutex_unlock(&mutex);
		arg = false;
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_CLKDIS, (void *)&arg, NULL);
		break;
	}
}

void hdmitx_hdcp_do_work(struct hdmitx_common *tx_comm)
{
	struct hdmitx20_dev *hdev =
		container_of(tx_comm, struct hdmitx20_dev, tx_comm);

	_hdcp_do_work(&hdev->work_do_hdcp.work);
}

int hdmitx_hdcp_esm_kill(struct hdmitx_common *tx_comm)
{
	int ret = 0;
	int i = 0;
	/* polling esm kill start 3 times */
	int polling_max_cnt = 3;

	if (!tx_comm)
		return -EINVAL;

	/* only for android platform which hdcp controlled by system_control */
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl != 0)
		return -EINVAL;

	if (tx_comm->hdcptx_comm.hdcp_mode == 2) {
		/* forcely notify hdcp_tx22 daemon to kill esm */
		tx_comm->hdcp_tst_sig = 1;

		for (i = 0; i < polling_max_cnt; i++) {
			/* block wait for hdcp_tx22 set hdcp_tst_sig = 0 by hdcp_pwr sysfs node */
			if (tx_comm->hdcp_tst_sig != 0) {
				msleep_interruptible(100);
			} else {
				/*
				 * after hdcp_tx22 set hdcp_tst_sig = 0,
				 * wait 100ms for ESM kill in hdcp_tx22
				 */
				msleep_interruptible(100);
				break;
			}
		}
		if (i == polling_max_cnt) {
			HDMITX_INFO("warning: hdcp_tx22 daemon not respond to kill esm!");
			ret = -1;
		}
		if (hdmitx_hw_cntl(tx_comm->tx_hw, HDCP22_GET_AXI_DMC_STS, NULL, NULL) == 0) {
			HDMITX_ERROR("hdcp2.2 esm not killed completely!\n");
			ret = -1;
		} else {
			HDMITX_DEBUG_HDCP("HDMI/HDCP AXI channel free\n");
			ret = 0;
		}
		/* recover not esm kill flag at last */
		tx_comm->hdcp_tst_sig = 0;
	}
	return ret;
}

static int read_buff(char *p)
{
	if (hdcplog_buf.rd_pos == hdcplog_buf.wr_pos)
		return 0;

	*p = hdcplog_buf.buf[hdcplog_buf.rd_pos];
	hdcplog_buf.rd_pos = (hdcplog_buf.rd_pos + 1) % HDCP_LOG_SIZE;
	return 1;
}

static void _write_buff(char c)
{
	hdcplog_buf.buf[hdcplog_buf.wr_pos] = c;
	hdcplog_buf.wr_pos = (hdcplog_buf.wr_pos + 1) % HDCP_LOG_SIZE;
	if (hdcplog_buf.wr_pos == hdcplog_buf.rd_pos)
		hdcplog_buf.rd_pos = (hdcplog_buf.rd_pos + 1) % HDCP_LOG_SIZE;
	wake_up_interruptible(&hdcplog_buf.wait);
}

static int write_buff(const char *fmt, ...)
{
	va_list args;
	int i, len;
	static char temp[64];

	va_start(args, fmt);
	len = vsnprintf(temp, sizeof(temp), fmt, args);
	va_end(args);

	for (i = 0; i < len; i++)
		_write_buff(temp[i]);

	return len;
}

static int hdcplog_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t hdcplog_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	ssize_t error = -EINVAL;
	int i = 0;
	char c;

	if ((file->f_flags & O_NONBLOCK) && hdcplog_buf.rd_pos == hdcplog_buf.wr_pos)
		return  -EAGAIN;

	if (!buf || !count)
		goto out;

	error = wait_event_interruptible(hdcplog_buf.wait,
			(hdcplog_buf.wr_pos != hdcplog_buf.rd_pos));
	if (error)
		goto out;
	while (!error && (read_buff(&c)) && i < count) {
		error = __put_user(c, buf);
		buf++;
		i++;
	}

	if (!error)
		error = i;
out:
	return error;
}

static void hdmitx20_set_hdcp_mode(struct hdmitx_common *tx_comm, const char *buf)
{
	enum hdmi_vic vic =
		hdmitx_hw_cntl(tx_comm->tx_hw, AUX_PKT_GET_AVI_VIC, NULL, NULL);
	u32 arg;

	if (hdmitx_hw_cntl(tx_comm->tx_hw, PLATFORM_RXSENSE, NULL, NULL) == 0)
		hdmitx_current_status(tx_comm, HDMITX_HDCP_DEVICE_NOT_READY_ERROR);
	/*
	 * there's risk:
	 * hdcp2.2 start auth-->enter early suspend, stop hdcp-->
	 * hdcp2.2 auth fail & timeout-->fall back to hdcp1.4, so
	 * hdcp running even no hdmi output-->resume, read EDID.
	 * EDID may read fail as hdcp may also access DDC simultaneously.
	 */
	mutex_lock(&tx_comm->hdmimode_mutex);
	if (!tx_comm->ready) {
		HDMITX_INFO("hdmi signal not ready, should not set hdcp mode %s\n", buf);
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	HDMITX_INFO(SYS "hdcp: set mode as %s\n", buf);
	arg = 1;
	hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_MUX_INIT, (void *)&arg, NULL);
	hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_GET_AUTH_RESULT, NULL, NULL);
	if (strncmp(buf, "0", 1) == 0) {
		tx_comm->hdcptx_comm.hdcp_mode = 0;
		arg = HDCP14_OFF;
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_MODE_OP, (void *)&arg, NULL);
		hdmitx_hdcp_do_work(tx_comm);
		hdmitx_current_status(tx_comm, HDMITX_HDCP_NOT_ENABLED);
	}
	if (strncmp(buf, "1", 1) == 0) {
		if (vic == HDMI_17_720x576p50_4x3 || vic == HDMI_18_720x576p50_16x9)
			usleep_range(500000, 500010);
		tx_comm->hdcptx_comm.hdcp_mode = 1;
		hdmitx_hdcp_do_work(tx_comm);
		arg = HDCP14_ON;
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_MODE_OP, (void *)&arg, NULL);
		hdmitx_current_status(tx_comm, HDMITX_HDCP_HDCP_1_ENABLED);
	}
	if (strncmp(buf, "2", 1) == 0) {
		if (tx_comm->efuse_dis_hdcp_tx22) {
			HDMITX_ERROR("warning, efuse disable hdcptx22\n");
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		tx_comm->hdcptx_comm.hdcp_mode = 2;
		hdmitx_hdcp_do_work(tx_comm);
		arg = 2;
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_MUX_INIT, (void *)&arg, NULL);
		hdmitx_current_status(tx_comm, HDMITX_HDCP_HDCP_2_ENABLED);
	}
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

static int hdmitx20_get_hdcp_ver(struct hdmitx_common *tx_comm, char *buf, int len)
{
	int pos = 0;
	u32 ver = hdcptx_get_rx_version(tx_comm);

	if (ver == 0x3)
		pos += snprintf(buf + pos, len - pos, "22\n\r");
	pos += snprintf(buf + pos, len - pos, "14\n\r");
	return pos;
}

static int hdmitx20_get_tx_hdcp_cap(struct hdmitx_common *tx_comm)
{
	unsigned int hdcptx_cap = 0, tx_cap = 0;
	struct hdcptx20_core_priv *p_hdcp;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return 0;
	}

	/* check hdcp key load status */
	hdcptx_cap = hdcptx_get_key_store(tx_comm);

	/* for android platform, always treat it as ready */
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0)
		return hdcptx_cap;
	if (!tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return 0;
	}
	p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;

	/* check additional private hdcp daemon status */
	switch (p_hdcp->hdcp22_daemon_state) {
	case HDCP22_DAEMON_DONE:
		tx_cap = hdcptx_cap & 0x3;
		HDMITX_DEBUG_HDCP("%s daemon done, return real tx_cap:%d\n", __func__, tx_cap);
		break;
	case HDCP22_DAEMON_TIMEOUT:
		tx_cap = hdcptx_cap & 0x1;
		HDMITX_DEBUG_HDCP("%s daemon timeout, return 1.4 tx_cap:%d\n", __func__, tx_cap);
		break;
	case HDCP22_DAEMON_LOADING:
	default:
		tx_cap = 0;
		HDMITX_DEBUG_HDCP("%s daemon loading, return null tx_cap:%d\n", __func__,
			tx_cap);
		break;
	}
	return tx_cap;
}

/* for linux/drm start */
static void drm_hdmitx_hdcp_disable(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm;
	struct hdcptx20_core_priv *p_hdcp;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0)
		return;
	if (!tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return;
	}

	hdcptx_comm = &tx_comm->hdcptx_comm;
	p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;

	if (!p_hdcp->hdcp_en)
		return;

	HDMITX_INFO("[%s]: %d\n", __func__, p_hdcp->hdcp_execute_type);
	/*
	 * when switch mode under hdcp1.4, should not
	 * trigger hdcp_tx22 daemon to change status.
	 * otherwise, it may exit idle state and
	 * enter polling(driver hdcp2.2 start) per 5ms
	 * which cause high cpu CPU utilization
	 */
	if (p_hdcp->hdcp_execute_type == HDCP_MODE22) {
		if (p_hdcp->hdcp_report == HDCP_TX22_START) {
			/* notify hdcp_tx22 to stop hdcp22 */
			p_hdcp->hdcp_report = HDCP_TX22_STOP;
			wake_up(&p_hdcp->hdcp_comm_queue);
			/* wait for hdcp_tx22 stop hdcp22 done */
			msleep_interruptible(200);
		}
		drm_hdmitx_disable_hdcp_mode(HDCP_MODE22);
		/* notify hdcp_tx22 to enter hdcp22 init state(DISCONNECT) */
		p_hdcp->hdcp_report = HDCP_TX22_DISCONNECT;
		wake_up(&p_hdcp->hdcp_comm_queue);
	} else if (p_hdcp->hdcp_execute_type == HDCP_MODE14) {
		drm_hdmitx_disable_hdcp_mode(HDCP_MODE14);
	}
	p_hdcp->hdcp_execute_type = HDCP_NULL;
	p_hdcp->hdcp_en = false;
	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
}

/*
 * currently for linux hdcp_tx22 daemon only,
 * note that android hdcp_tx22 will also read it
 */
bool is_hdcp22_stop_state(struct hdmitx_common *tx_comm)
{
	struct hdcptx20_core_priv *p_hdcp;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return false;
	}
	/* add logic if need to be used in android in the future */
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0)
		return false;
	if (!tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return false;
	}
	p_hdcp = tx_comm->hdcptx_priv;

	return p_hdcp->hdcp_report == HDCP_TX22_STOP;
}

static void drm_hdmitx_hdcp_disconnect(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm;
	struct hdcptx20_core_priv *p_hdcp;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0)
		return;
	if (!tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;
	p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;

	p_hdcp->hdcp_report = HDCP_TX22_DISCONNECT;
	p_hdcp->hdcp_execute_type = HDCP_NULL;
	p_hdcp->hdcp_en = false;
	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	HDMITX_INFO("[%s]: HDCP_TX22_DISCONNECT\n", __func__);
	wake_up(&p_hdcp->hdcp_comm_queue);
}

static void drm_hdmitx_hdcp_enable(struct hdmitx_common *tx_comm, int hdcp_type)
{
	struct hdcptx_common *hdcptx_comm;
	struct hdcptx20_core_priv *p_hdcp;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0)
		return;
	if (!tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;
	p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;

	if (hdcp_type == HDCP_NULL)
		return;

	/* hdcp enabled, but may not start auth as key not ready */
	p_hdcp->hdcp_en = true;
	p_hdcp->hdcp_execute_type = hdcp_type;
	hdcptx_comm->hdcp_fail_cnt = 0;
	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;

	if (p_hdcp->hdcp_execute_type == HDCP_MODE22) {
		if (p_hdcp->hdcp22_daemon_state != HDCP22_DAEMON_DONE) {
			HDMITX_INFO("[%s]: hdcp_tx22 not ready, delay hdcp auth\n", __func__);
			return;
		}
		drm_hdmitx_enable_hdcp_mode(2);
		p_hdcp->hdcp_report = HDCP_TX22_START;
		msleep(50);
		wake_up(&p_hdcp->hdcp_comm_queue);
	} else if (p_hdcp->hdcp_execute_type == HDCP_MODE14) {
		drm_hdmitx_enable_hdcp_mode(1);
	}
	HDMITX_INFO("[%s]: report=%d, execute=%u\n",
		 __func__, p_hdcp->hdcp_report, p_hdcp->hdcp_execute_type);
}

static long hdcp_comm_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int ret;
	unsigned int out_size;
	struct hdcptx_common *hdcptx_comm = file->private_data;
	struct hdmitx_common *tx_comm;
	struct hdcptx20_core_priv *p_hdcp;

	if (!hdcptx_comm) {
		HDMITX_ERROR("%s NULL hdcptx_comm instance\n", __func__);
		return -EINVAL;
	}
	tx_comm = container_of(hdcptx_comm, struct hdmitx_common, hdcptx_comm);
	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return -EINVAL;
	}
	p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;

	switch (cmd) {
	case TEE_HDCP_IOC_START:
		/* notify by TEE, hdcp key ready, echo 2 > hdcp_mode */
		ret = 0;
		if (hdcptx_get_key_store(tx_comm) & 0x2) {
			/*
			 * when bootup, if hdcp22 init after hdcp14 auth,
			 * hdcp path will switch to hdcp22. need to delay
			 * hdcp auth to cover this issue.
			 */
			drm_tx_hdcp22_init(tx_comm);
		}
		HDMITX_INFO("hdcp key load ready\n");
		break;
	case HDCP_DAEMON_IOC_LOAD_END:
		/* hdcp_tx22 load ready (after TEE key ready) */
		HDMITX_INFO("IOC_LOAD_END %d, %d\n",
			 p_hdcp->hdcp_report, p_hdcp->hdcp_poll_report);
		if (p_hdcp->hdcp22_daemon_state == HDCP22_DAEMON_TIMEOUT)
			HDMITX_INFO("hdcp22 key load late than TIMEOUT.\n");
		p_hdcp->hdcp22_daemon_state = HDCP22_DAEMON_DONE;
		p_hdcp->hdcp_poll_report = p_hdcp->hdcp_report;
		ret = 0;
		break;
	case HDCP_DAEMON_IOC_REPORT:
		ret = copy_to_user((void __user *)arg,
				       (void *)&p_hdcp->hdcp_report,
				       sizeof(p_hdcp->hdcp_report));
		if (ret != 0) {
			out_size = sizeof(p_hdcp->hdcp_report);
			HDMITX_INFO("out_size: %u, left_size: %u\n",
				 out_size, ret);
			ret = -1;
		}
		break;
	default:
		ret = -EPERM;
	}
	return ret;
}

static int hdcp_communicate_open(struct inode *inode, struct file *file)
{
	struct hdcptx_common *hdcptx_comm = container_of(file->private_data,
		struct hdcptx_common, hdcp_misc_dev);

	file->private_data = hdcptx_comm;
	return 0;
}

static int hdcp_communicate_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static unsigned int hdcp_comm_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct hdcptx_common *hdcptx_comm = file->private_data;
	struct hdmitx_common *tx_comm;
	struct hdcptx20_core_priv *p_hdcp;

	if (!hdcptx_comm) {
		HDMITX_ERROR("%s NULL hdcptx_comm instance\n", __func__);
		return -EINVAL;
	}
	tx_comm = container_of(hdcptx_comm, struct hdmitx_common, hdcptx_comm);
	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return -EINVAL;
	}
	p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;

	HDMITX_INFO("hdcp_poll %d, %d\n", p_hdcp->hdcp_report, p_hdcp->hdcp_poll_report);
	poll_wait(file, &p_hdcp->hdcp_comm_queue, wait);
	if (p_hdcp->hdcp_report != p_hdcp->hdcp_poll_report) {
		mask = POLLIN | POLLRDNORM;
		p_hdcp->hdcp_poll_report = p_hdcp->hdcp_report;
	}
	return mask;
}

static const struct file_operations hdcp_comm_file_operations = {
	.owner = THIS_MODULE,
	.open = hdcp_communicate_open,
	.release = hdcp_communicate_release,
	.unlocked_ioctl = hdcp_comm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdcp_comm_ioctl,
#endif
	.poll = hdcp_comm_poll,
};

static void drm_hdcptx_key_notify(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hdcptx20_core_priv *p_hdcp =
			container_of(dwork, struct hdcptx20_core_priv, notify_work);
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;

	tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify(tx_comm->hdcptx_comm.tx_hdcp_cb.data,
		HDCP_KEY_UPDATE, HDCP_AUTH_UNKNOWN);
}

static void drm_hdcp_tx22_daemon_check(struct timer_list *timer)
{
	struct hdcptx20_core_priv *p_hdcp =
		container_of(timer, struct hdcptx20_core_priv, daemon_load_timer);

	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return;
	}
	/*send notify when key load finished or timeout.*/
	if (p_hdcp->hdcp22_daemon_state == HDCP22_DAEMON_DONE) {
		HDMITX_INFO("hdcp_tx22 load ready, stop timer\n");
		schedule_delayed_work(&p_hdcp->notify_work, msecs_to_jiffies(10));
	} else if (p_hdcp->key_chk_cnt++ < TIMER_CHK_CNT) {
		p_hdcp->daemon_load_timer.expires = jiffies + TIMER_CHECK;
		add_timer(&p_hdcp->daemon_load_timer);
	} else {
		HDMITX_INFO("hdcp_tx22 load timeout\n");
		p_hdcp->hdcp22_daemon_state = HDCP22_DAEMON_TIMEOUT;
		schedule_delayed_work(&p_hdcp->notify_work, 0);
	}
}

static void drm_hdmitx_hdcp_init(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm;
	struct hdcptx20_core_priv *p_hdcp;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0)
		return;
	if (!tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;
	p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;

	/* init common part */
	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	hdcptx_comm->tx_hdcp_cb.hdcp_notify = NULL;
	hdcptx_comm->tx_hdcp_cb.data = NULL;

	/* init private part */
	p_hdcp->hdcp_report = HDCP_TX22_DISCONNECT;
	p_hdcp->hdcp_en = false;
	/* p_hdcp->hdcp_execute_type = 0; */
}

static void drm_hdmitx_hdcp_exit(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0)
		return;
	hdcptx_comm = &tx_comm->hdcptx_comm;

	/* reset common part, keep private part */
	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	hdcptx_comm->tx_hdcp_cb.hdcp_notify = NULL;
	hdcptx_comm->tx_hdcp_cb.data = NULL;
}

/* after TEE hdcp key valid, do hdcp22 init before tx22 start */
static void drm_tx_hdcp22_init(struct hdmitx_common *tx_comm)
{
	u32 arg = 2;

	hdmitx_hdcp_do_work(tx_comm);
	hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_MUX_INIT, (void *)&arg, NULL);
}

/* echo 1/2 > hdcp_mode */
static void drm_hdmitx_enable_hdcp_mode(unsigned int content_type)
{
	struct hdmitx20_dev *hdev = get_hdmitx20_device();
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
	u32 arg;

	vic = hdmitx_hw_cntl(&hdev->hw_comm, AUX_PKT_GET_AVI_VIC, NULL, NULL);
	hdmitx_hw_cntl(&hdev->hw_comm, HDCP_GET_AUTH_RESULT, NULL, NULL);

	if (content_type == 1) {
		arg = 1;
		hdmitx_hw_cntl(&hdev->hw_comm, HDCP_MUX_INIT, (void *)&arg, NULL);
		if (vic == HDMI_17_720x576p50_4x3 || vic == HDMI_18_720x576p50_16x9)
			usleep_range(500000, 500010);
		hdev->tx_comm.hdcptx_comm.hdcp_mode = 1;
		hdmitx_hdcp_do_work(&hdev->tx_comm);
		arg = HDCP14_ON;
		hdmitx_hw_cntl(&hdev->hw_comm, HDCP_MODE_OP, (void *)&arg, NULL);
	} else if (content_type == 2) {
		hdev->tx_comm.hdcptx_comm.hdcp_mode = 2;
		hdmitx_hdcp_do_work(&hdev->tx_comm);
		/*
		 * for drm hdcp_tx22, esm init only once
		 * don't do HDCP22 IP reset after init done!
		 */
		arg = 3;
		hdmitx_hw_cntl(&hdev->hw_comm, HDCP_MUX_INIT, (void *)&arg, NULL);
	}
}

/* echo -1 > hdcp_mode;echo stop14/22 > hdcp_ctrl */
static void drm_hdmitx_disable_hdcp_mode(unsigned int content_type)
{
	struct hdmitx20_dev *hdev = get_hdmitx20_device();
	u32 arg = 1;

	hdmitx_hw_cntl(&hdev->hw_comm, HDCP_MUX_INIT, (void *)&arg, NULL);
	hdmitx_hw_cntl(&hdev->hw_comm, HDCP_GET_AUTH_RESULT, NULL, NULL);

	if (content_type == 1) {
		arg = HDCP14_OFF;
		hdmitx_hw_cntl(&hdev->hw_comm, HDCP_MODE_OP, (void *)&arg, NULL);
	} else if (content_type == 2) {
		arg = HDCP22_OFF;
		hdmitx_hw_cntl(&hdev->hw_comm, HDCP_MODE_OP, (void *)&arg, NULL);
	}

	hdev->tx_comm.hdcptx_comm.hdcp_mode = 0;
	hdmitx_hdcp_do_work(&hdev->tx_comm);
	hdmitx_current_status(&hdev->tx_comm, HDMITX_HDCP_NOT_ENABLED);
}

/* for linux/drm end */

static void hdcptx_20_ctrl_ops_init(struct hdcptx_common *hdcptx_comm)
{
	if (!hdcptx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	hdcptx_comm->drm_hdcp_init = drm_hdmitx_hdcp_init;
	hdcptx_comm->drm_hdcp_exit = drm_hdmitx_hdcp_exit;
	hdcptx_comm->drm_hdcp_enable = drm_hdmitx_hdcp_enable;
	hdcptx_comm->drm_hdcp_disable = drm_hdmitx_hdcp_disable;
	hdcptx_comm->drm_hdcp_disconnect = drm_hdmitx_hdcp_disconnect;

	hdcptx_comm->set_hdcp_mode = hdmitx20_set_hdcp_mode;
	hdcptx_comm->get_hdcp_ver = hdmitx20_get_hdcp_ver;
	hdcptx_comm->get_tx_hdcp_cap = hdmitx20_get_tx_hdcp_cap;
}

const struct file_operations hdcplog_ops = {
	.read = hdcplog_read,
	.open = hdcplog_open,
};

int hdmitx20_hdcp_init(struct hdmitx20_dev *hdev)
{
	struct dentry *entry;
	struct hdcptx20_core_priv *p_hdcp;
	struct hdmitx_common *tx_comm = &hdev->tx_comm;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return -ENODEV;
	}

	hdcptx_20_ctrl_ops_init(&tx_comm->hdcptx_comm);
	esm_init();
	memset(&hdcplog_buf, 0, sizeof(hdcplog_buf));
	init_waitqueue_head(&hdcplog_buf.wait);
	tx_comm->hdcptx_comm.task_hdcp = kthread_run(hdmitx_hdcp_stat_monitor_task,
		(void *)tx_comm, "kthread_hdcp");
	INIT_DELAYED_WORK(&hdev->work_do_hdcp, _hdcp_do_work);

	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl > 0) {
		p_hdcp = kzalloc(sizeof(*p_hdcp), GFP_KERNEL);
		if (!p_hdcp) {
			HDMITX_ERROR("%s no enough mem for hdcp private struct\n", __func__);
			return -ENOMEM;
		}
		INIT_DELAYED_WORK(&p_hdcp->notify_work, drm_hdcptx_key_notify);
		p_hdcp->hdcp22_daemon_state = HDCP22_DAEMON_LOADING;
		p_hdcp->key_chk_cnt = 0;
		p_hdcp->bind_instance = tx_comm;
		init_waitqueue_head(&p_hdcp->hdcp_comm_queue);
		timer_setup(&p_hdcp->daemon_load_timer, drm_hdcp_tx22_daemon_check, 0);
		p_hdcp->daemon_load_timer.expires = jiffies + TIMER_CHECK;
		add_timer(&p_hdcp->daemon_load_timer);

		tee_comm_dev_reg(&tx_comm->hdcptx_comm.hdcp_misc_dev, &hdcp_comm_file_operations);
		tx_comm->hdcptx_priv = p_hdcp;
	}
	if (!tx_comm->hdmitx_file_dbgfs)
		tx_comm->hdmitx_file_dbgfs = debugfs_create_dir(DEVICE_NAME, NULL);
	if (!tx_comm->hdmitx_file_dbgfs) {
		HDMITX_ERROR("can't create %s debugfs dir\n", DEVICE_NAME);
		return 0;
	}
	entry = debugfs_create_file("hdcp_log", S_IFREG | 0444,
			tx_comm->hdmitx_file_dbgfs, NULL,
			&hdcplog_ops);
	if (!entry)
		HDMITX_ERROR("debugfs create file %s failed\n", "hdcp_log");

	return 0;
}

void hdmitx20_hdcp_uninit(struct hdmitx20_dev *hdev)
{
	struct hdcptx20_core_priv *p_hdcp;
	struct hdmitx_common *tx_comm = &hdev->tx_comm;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	esm_exit();
	kthread_stop(tx_comm->hdcptx_comm.task_hdcp);
	cancel_delayed_work_sync(&hdev->work_do_hdcp);
	kfree(tx_comm->topo_info);
	tx_comm->topo_info = NULL;
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl > 0) {
		p_hdcp = (struct hdcptx20_core_priv *)tx_comm->hdcptx_priv;
		if (p_hdcp) {
			del_timer_sync(&p_hdcp->daemon_load_timer);
			cancel_delayed_work(&p_hdcp->notify_work);
			tee_comm_dev_unreg(&tx_comm->hdcptx_comm.hdcp_misc_dev);
			kfree(p_hdcp);
		}
		tx_comm->hdcptx_priv = NULL;
	}
}

