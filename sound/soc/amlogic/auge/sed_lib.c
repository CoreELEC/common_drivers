// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/clk-provider.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/suspend.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/firmware.h>
#include <linux/irq.h>

#include "sed_lib.h"

static int snd_pcm_lib_read_transfer(struct snd_pcm_substream *substream,
					 unsigned int hwoff,
					 void *data, unsigned int off,
					 snd_pcm_uframes_t frames)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	char *buf = (char *)data + frames_to_bytes(runtime, off);
	char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, hwoff);

	/*sram use the memcpy_toio*/
	memcpy(buf, hwbuf, frames_to_bytes(runtime, frames));
	return 0;
}

static int aml_wait_for_avail(struct snd_pcm_substream *substream,
				  snd_pcm_uframes_t *availp)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	wait_queue_entry_t wait;
	int err = 0;
	snd_pcm_uframes_t avail = 0;
	long wait_time, tout;

	init_waitqueue_entry(&wait, current);
	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&runtime->tsleep, &wait);

	if (runtime->no_period_wakeup) {
		wait_time = MAX_SCHEDULE_TIMEOUT;
	} else {
		/* use wait time from substream if available */
		if (substream->wait_time) {
			wait_time = substream->wait_time;
		} else {
			wait_time = 100;

			if (runtime->rate) {
				long t = runtime->buffer_size * 1100 / runtime->rate;

				wait_time = max(t, wait_time);
			}
		}
		wait_time = msecs_to_jiffies(wait_time);
	}
	for (;;) {
		if (signal_pending(current)) {
			err = -ERESTARTSYS;
			break;
		}

		/*
		 * We need to check if space became available already
		 * (and thus the wakeup happened already) first to close
		 * the race of space already having become available.
		 * This check must happen after been added to the waitqueue
		 * and having current state be INTERRUPTIBLE.
		 */
		avail = snd_pcm_capture_avail(substream->runtime);
		if (avail >= runtime->twake)
			break;
		tout = schedule_timeout(wait_time);
		set_current_state(TASK_INTERRUPTIBLE);
		switch (runtime->state) {
		case SNDRV_PCM_STATE_SUSPENDED:
			err = -ESTRPIPE;
			goto _endloop;
		case SNDRV_PCM_STATE_XRUN:
			err = -EPIPE;
			goto _endloop;
		case SNDRV_PCM_STATE_DRAINING:
			if (is_playback)
				err = -EPIPE;
			else
				avail = 0; /* indicate draining */
			goto _endloop;
		case SNDRV_PCM_STATE_OPEN:
		case SNDRV_PCM_STATE_SETUP:
		case SNDRV_PCM_STATE_DISCONNECTED:
			err = -EBADFD;
			goto _endloop;
		case SNDRV_PCM_STATE_PAUSED:
			continue;
		}
		if (!tout) {
			pcm_dbg(substream->pcm,
				"%s timeout (DMA or IRQ trouble?)\n",
				is_playback ? "playback write" : "capture read");
			err = -EIO;
			break;
		}
	}
 _endloop:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&runtime->tsleep, &wait);
	*availp = avail;
	return err;
}

int aml_snd_pcm_lib_xfer(struct snd_pcm_substream *substream, void *data,
		snd_pcm_uframes_t size)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_uframes_t offset = 0;
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_uframes_t avail;
	int err = 0;
	int ret = 0;

	if (size == 0)
		return 0;
	if (runtime->state == SNDRV_PCM_STATE_PREPARED) {
		ret = snd_pcm_kernel_ioctl(substream, SNDRV_PCM_IOCTL_START, NULL);
		if (ret < 0) {
			pr_err("start error");
			return ret;
		}
	}
	switch (runtime->status->state) {
	case SNDRV_PCM_STATE_XRUN:
		err = -EPIPE;
		goto _end_unlock;
	case SNDRV_PCM_STATE_SUSPENDED:
		err = -ESTRPIPE;
		goto _end_unlock;
	default:
		break;
	}
	avail = snd_pcm_capture_avail(runtime);
	while (size > 0) {
		snd_pcm_uframes_t frames, appl_ptr, appl_ofs;
		snd_pcm_uframes_t cont;

		if (!avail) {
			err = aml_wait_for_avail(substream, &avail);
			if (err < 0)
				goto _end_unlock;
		}
		frames = size > avail ? avail : size;
		cont =
			runtime->buffer_size -
			runtime->control->appl_ptr % runtime->buffer_size;
		if (frames > cont)
			frames = cont;
		appl_ptr = runtime->control->appl_ptr;
		appl_ofs = appl_ptr % runtime->buffer_size;
		snd_pcm_lib_read_transfer(substream, appl_ofs, data, offset,
				frames);
		switch (runtime->status->state) {
		case SNDRV_PCM_STATE_XRUN:
			err = -EPIPE;
			goto _end_unlock;
		case SNDRV_PCM_STATE_SUSPENDED:
			err = -ESTRPIPE;
			goto _end_unlock;
		default:
			break;
		}
		appl_ptr += frames;
		if (appl_ptr >= runtime->boundary)
			appl_ptr -= runtime->boundary;
		runtime->control->appl_ptr = appl_ptr;
		offset += frames;
		size -= frames;
		xfer += frames;
		avail -= frames;
	}
_end_unlock:
	return xfer > 0 ? (snd_pcm_sframes_t)xfer : err;
}
