// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright 2026 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * KFD RDMA-pin bounded-wait helpers + orphan reaper.
 *
 * This file introduces the KFD-side bounded-wait helpers and orphan-pin
 * reaper that back five module parameters introduced in amdgpu_drv.c:
 *
 *   amdgpu_kfd_unpin_drain_ms      strict-drain timeout in unpin path
 *   amdgpu_kfd_free_wait_ms        bounded wait on pin_count==0 in free path
 *   amdgpu_kfd_free_on_pinned      policy when hipFree hits a pinned BO
 *                                  (0 = silent orphan / 1 = bounded wait
 *                                  then queue for reaper)
 *   amdgpu_pin_orphan_timeout_ms   age threshold for the reaper to force-
 *                                  unpin a queued orphan
 *   amdgpu_pin_reaper_interval_ms  delayed-work tick of the reaper
 *
 * All five default to 0 (feature off).  When none are set, the helpers
 * here are unreachable and the reaper background worker is not started.
 *
 * Motivation: multi-tenant HIP serving stacks observe RDMA peer drivers
 * holding pin_count > 0 on KFD BOs across the hipFree() call.  Without
 * a bounded-wait policy the BO is silently orphaned (memory accounting
 * leaks until process exit) or the unpin path can deadlock waiting for
 * a wedged peer driver.  The opt-in knobs let operators choose a
 * survival policy (bounded wait + reaper) without changing default
 * behaviour for stock callers.
 */

#include <linux/delay.h>
#include <linux/dma-resv.h>
#include <linux/list.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#include "amdgpu.h"
#include "amdgpu_amdkfd.h"
#include "amdgpu_object.h"

/* ---- bounded waits ----------------------------------------------------- */

/**
 * amdgpu_kfd_unpin_drain - bounded dma_resv_wait_timeout on a BO's resv.
 * @bo: BO whose resv to drain.  Caller must hold bo reserved.
 * @timeout_ms: deadline in milliseconds; <= 0 returns 0 immediately.
 *
 * Mirrors the bounded-wait semantics needed by the unpin path: wait for
 * every fence on the BO's reservation (including non-KFD-owned ones)
 * before letting pin_count drop, so an in-flight RDMA WR cannot race
 * with the unpin and fault peer drivers.
 *
 * Return: 0 on drained, -ETIME on timeout, negative errno on wait error.
 */
int amdgpu_kfd_unpin_drain(struct amdgpu_bo *bo, int timeout_ms)
{
	struct dma_resv *resv;
	long timeout;
	long left;

	if (!bo || timeout_ms <= 0)
		return 0;

	resv = bo->tbo.base.resv;
	if (!resv)
		return 0;

	timeout = msecs_to_jiffies(timeout_ms);
	left = dma_resv_wait_timeout(resv, DMA_RESV_USAGE_BOOKKEEP,
				     true, timeout);
	if (left == 0) {
		struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
		atomic64_inc(&adev->kfd.unpin_drain_timeouts);
		dev_warn_ratelimited(adev->dev,
			"amdgpu_kfd_unpin_drain timeout bo=%p size=%lluMB timeout=%dms\n",
			bo, (u64)amdgpu_bo_size(bo) >> 20, timeout_ms);
		return -ETIME;
	} else if (left < 0) {
		return (int)left;
	}
	return 0;
}

/**
 * amdgpu_kfd_wait_pin_drop - poll bo->tbo.pin_count until 0 or timeout.
 * @bo: BO to observe.  Caller must hold bo reserved.
 * @timeout_ms: deadline in milliseconds; <= 0 returns -EBUSY iff pinned.
 *
 * Exponential-backoff sleep (100us .. 5ms) until pin_count drops to 0
 * or the wall-clock deadline elapses.
 *
 * Return: 0 on pin dropped, -EBUSY on still-pinned at deadline.
 */
int amdgpu_kfd_wait_pin_drop(struct amdgpu_bo *bo, int timeout_ms)
{
	unsigned long deadline;
	int slept_us = 0;

	if (!bo || timeout_ms <= 0)
		return (bo && bo->tbo.pin_count) ? -EBUSY : 0;

	if (bo->tbo.pin_count == 0)
		return 0;

	deadline = jiffies + msecs_to_jiffies(timeout_ms);

	while (time_before(jiffies, deadline)) {
		/* exponential backoff: 100us, 200us, ..., capped 5ms */
		int us = min(5000, 100 << min(slept_us / 5, 6));

		usleep_range(us, us + (us >> 2));
		slept_us++;

		if (bo->tbo.pin_count == 0)
			return 0;
	}

	return -EBUSY;
}

/* ---- orphan queue + reaper -------------------------------------------- */

static void amdgpu_kfd_reaper_fn(struct work_struct *work);

/**
 * amdgpu_kfd_orphan_queue - queue a still-pinned BO for the reaper to
 *                           force-unpin later.
 * @adev: device whose kfd.orphan_list owns the queued node.
 * @bo:   BO to queue.  Caller takes a ref; this function takes a second
 *        ref to keep the BO alive until the reaper runs.
 * @bytes: BO size for accounting in stats / dev_warn.
 *
 * Userspace's hipFree() succeeds and returns immediately; the BO sits
 * on adev->kfd.orphan_list until amdgpu_pin_orphan_timeout_ms elapses
 * and the reaper walks the list to force-unpin.
 *
 * Return: 0 on queued, negative errno on alloc / arg failure.
 */
int amdgpu_kfd_orphan_queue(struct amdgpu_device *adev, struct amdgpu_bo *bo,
			    u64 bytes)
{
	struct kfd_orphan_pin *op;

	if (!adev || !bo)
		return -EINVAL;

	op = kzalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		return -ENOMEM;

	amdgpu_bo_ref(bo);
	op->bo = bo;
	op->queued_at_jiffies = jiffies;
	op->bytes = bytes;
	op->owner_pid = current->pid;
	strscpy(op->owner_comm, current->comm, sizeof(op->owner_comm));

	spin_lock(&adev->kfd.orphan_lock);
	list_add_tail(&op->node, &adev->kfd.orphan_list);
	spin_unlock(&adev->kfd.orphan_lock);

	atomic64_inc(&adev->kfd.rdma_pin_orphans_queued);

	dev_warn_ratelimited(adev->dev,
		"amdgpu_kfd: orphan queued bo=%p size=%lluMB pid=%d(%s) pin_count=%d; force-unpin after %dms\n",
		bo, bytes >> 20, op->owner_pid, op->owner_comm,
		bo->tbo.pin_count, amdgpu_pin_orphan_timeout_ms);

	/* Kick the reaper in case it was sleeping. */
	if (adev->kfd.reaper_started)
		mod_delayed_work(system_wq, &adev->kfd.reaper_work, 0);
	return 0;
}

/*
 * Reaper worker.  Walks adev->kfd.orphan_list, moves nodes older than
 * amdgpu_pin_orphan_timeout_ms into a private list, then for each one
 * reserves the BO, drains its resv via amdgpu_kfd_unpin_drain(), force-
 * decrements pin_count until 0, and releases.
 *
 * Reschedules itself every amdgpu_pin_reaper_interval_ms until
 * amdgpu_kfd_reaper_stop() is called.
 */
static void amdgpu_kfd_reaper_fn(struct work_struct *work)
{
	struct amdgpu_kfd_dev *kfd = container_of(to_delayed_work(work),
				struct amdgpu_kfd_dev, reaper_work);
	struct amdgpu_device *adev = container_of(kfd, struct amdgpu_device,
						   kfd);
	struct kfd_orphan_pin *op, *tmp;
	LIST_HEAD(reap_now);
	unsigned long cutoff;
	int reaped = 0;

	if (amdgpu_pin_orphan_timeout_ms <= 0)
		goto reschedule;

	cutoff = jiffies - msecs_to_jiffies(amdgpu_pin_orphan_timeout_ms);

	spin_lock(&kfd->orphan_lock);
	list_for_each_entry_safe(op, tmp, &kfd->orphan_list, node) {
		if (time_after_eq(op->queued_at_jiffies, cutoff))
			break; /* list ordered by queue time */
		list_move_tail(&op->node, &reap_now);
	}
	spin_unlock(&kfd->orphan_lock);

	list_for_each_entry_safe(op, tmp, &reap_now, node) {
		struct amdgpu_bo *bo = op->bo;
		int r;

		dev_warn(adev->dev,
			"amdgpu_kfd: reaping orphan bo=%p size=%lluMB aged %ums pid=%d(%s) pin_count=%d\n",
			bo, op->bytes >> 20,
			jiffies_to_msecs(jiffies - op->queued_at_jiffies),
			op->owner_pid, op->owner_comm, bo->tbo.pin_count);

		r = amdgpu_bo_reserve(bo, false);
		if (r) {
			dev_err_ratelimited(adev->dev,
				"amdgpu_kfd: reap reserve failed bo=%p r=%d\n",
				bo, r);
		} else {
			(void)amdgpu_kfd_unpin_drain(bo,
				amdgpu_kfd_unpin_drain_ms);

			while (bo->tbo.pin_count > 0)
				amdgpu_bo_unpin(bo);

			if (bo->tbo.resource &&
			    bo->tbo.resource->mem_type == TTM_PL_VRAM)
				atomic64_sub((s64)amdgpu_bo_size(bo),
					&adev->kfd.vram_pinned);

			amdgpu_bo_unreserve(bo);
		}

		amdgpu_bo_unref(&op->bo);
		atomic64_inc(&kfd->rdma_pin_orphans_reaped);
		list_del(&op->node);
		kfree(op);
		reaped++;
	}

	if (reaped)
		dev_info(adev->dev,
			"amdgpu_kfd: reaped %d orphan(s) queued_total=%lld reaped_total=%lld\n",
			reaped,
			(long long)atomic64_read(&kfd->rdma_pin_orphans_queued),
			(long long)atomic64_read(&kfd->rdma_pin_orphans_reaped));

reschedule:
	if (kfd->reaper_started && amdgpu_pin_reaper_interval_ms > 0)
		schedule_delayed_work(&kfd->reaper_work,
			msecs_to_jiffies(amdgpu_pin_reaper_interval_ms));
}

/**
 * amdgpu_kfd_reaper_start - initialise orphan list and start reaper.
 * @adev: device whose kfd to start.
 *
 * Idempotent.  Called from amdgpu_amdkfd_device_init().  No-op when
 * adev->kfd has already been started.
 */
void amdgpu_kfd_reaper_start(struct amdgpu_device *adev)
{
	struct amdgpu_kfd_dev *kfd = &adev->kfd;

	if (kfd->reaper_started)
		return;

	spin_lock_init(&kfd->orphan_lock);
	INIT_LIST_HEAD(&kfd->orphan_list);
	INIT_DELAYED_WORK(&kfd->reaper_work, amdgpu_kfd_reaper_fn);
	atomic64_set(&kfd->rdma_pin_orphans_queued, 0);
	atomic64_set(&kfd->rdma_pin_orphans_reaped, 0);
	atomic64_set(&kfd->free_wait_pinned_count, 0);
	atomic64_set(&kfd->free_wait_pinned_timeout, 0);
	atomic64_set(&kfd->unpin_drain_timeouts, 0);
	kfd->reaper_started = true;

	if (amdgpu_pin_reaper_interval_ms > 0)
		schedule_delayed_work(&kfd->reaper_work,
			msecs_to_jiffies(amdgpu_pin_reaper_interval_ms));

	dev_info(adev->dev,
		"amdgpu_kfd: pin-orphan reaper started: orphan_timeout=%dms interval=%dms\n",
		amdgpu_pin_orphan_timeout_ms, amdgpu_pin_reaper_interval_ms);
}

/**
 * amdgpu_kfd_reaper_stop - cancel reaper and drain orphan list.
 * @adev: device whose kfd to stop.
 *
 * Idempotent.  Called from amdgpu_amdkfd_device_fini_sw().  Anything
 * still on the orphan list is reaped here unconditionally regardless
 * of age, since the device is going away.
 */
void amdgpu_kfd_reaper_stop(struct amdgpu_device *adev)
{
	struct amdgpu_kfd_dev *kfd = &adev->kfd;
	struct kfd_orphan_pin *op, *tmp;
	LIST_HEAD(drain);

	if (!kfd->reaper_started)
		return;

	kfd->reaper_started = false;
	cancel_delayed_work_sync(&kfd->reaper_work);

	spin_lock(&kfd->orphan_lock);
	list_splice_init(&kfd->orphan_list, &drain);
	spin_unlock(&kfd->orphan_lock);

	list_for_each_entry_safe(op, tmp, &drain, node) {
		struct amdgpu_bo *bo = op->bo;
		int r = amdgpu_bo_reserve(bo, false);

		if (!r) {
			while (bo->tbo.pin_count > 0)
				amdgpu_bo_unpin(bo);
			amdgpu_bo_unreserve(bo);
		}
		amdgpu_bo_unref(&op->bo);
		list_del(&op->node);
		kfree(op);
	}

	dev_info(adev->dev,
		"amdgpu_kfd: pin-orphan reaper stopped: queued=%lld reaped=%lld unpin_drain_to=%lld free_wait=%lld free_wait_to=%lld\n",
		(long long)atomic64_read(&kfd->rdma_pin_orphans_queued),
		(long long)atomic64_read(&kfd->rdma_pin_orphans_reaped),
		(long long)atomic64_read(&kfd->unpin_drain_timeouts),
		(long long)atomic64_read(&kfd->free_wait_pinned_count),
		(long long)atomic64_read(&kfd->free_wait_pinned_timeout));
}
