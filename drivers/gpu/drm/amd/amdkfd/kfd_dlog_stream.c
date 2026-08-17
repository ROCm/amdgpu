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
 *
 * KFD dispatch-log profiler stream (anon_inode fd; KFD-owned VMID0 GTT BO,
 * consumed zero-copy via mmap).
 */

#include <linux/anon_inodes.h>
#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/pid_namespace.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <drm/ttm/ttm_tt.h>
#include <uapi/linux/kfd_ioctl.h>
#include "amdgpu_object.h"
#include "amdgpu_amdkfd.h"
#include "kfd_priv.h"

#define KFD_DLOG_STREAM_NAME	"kfd_dlog_stream"

/* Level-triggered poll wake set shared by every stream wake path. */
#define DLOG_WAKE_MASK	(EPOLLIN | EPOLLRDNORM | EPOLLHUP)

/*
 * Aggregate pinned-GTT cap across all streams: OPEN_STREAM pins a BO with no
 * per-process accounting, so bound a hostile caller pinning many streams.
 */
#define KFD_DLOG_PINNED_GTT_LIMIT	(256ULL << 20)
static atomic64_t kfd_dlog_pinned_gtt = ATOMIC64_INIT(0);

/* Reserve @bytes against the global pinned-GTT cap; false if it would exceed. */
static bool kfd_dlog_gtt_reserve(u64 bytes)
{
	u64 now = atomic64_add_return(bytes, &kfd_dlog_pinned_gtt);

	if (now > KFD_DLOG_PINNED_GTT_LIMIT) {
		atomic64_sub(bytes, &kfd_dlog_pinned_gtt);
		return false;
	}
	return true;
}

static void kfd_dlog_gtt_unreserve(u64 bytes)
{
	atomic64_sub(bytes, &kfd_dlog_pinned_gtt);
}

struct kfd_dlog_stream {
	struct kfd_process	*target;
	/*
	 * RAW owning node, read only under @lock; NULLed at the last kref put.
	 * ACCEPTED use-after-free on device removal, matching the rest of KFD.
	 */
	struct kfd_node		*node;
	u32			gpu_id;
	u32			target_pid;
	/* IH wake-routing key, snapshot of pdd->pasid; assumed stable. */
	u32			pasid;

	wait_queue_head_t	poll_wq;
	/*
	 * Set under @lock by detach_locked() once every raw pointer (@node/@bo/
	 * @wptr/@rptr) is NULLed; read lock-free to reject a detached stream.
	 */
	bool			torn_down;
	/* Target gone (exit), HUP published; write-once under st->lock. */
	bool			terminal;

	/*
	 * KFD-owned GTT BO (pinned+GART-bound+kmapped): stable VMID0 @gart_addr.
	 * Freed in exactly one place, the detach at the last kref put.
	 */
	struct amdgpu_bo	*bo;
	/* True once bo_size was reserved against the global pinned-GTT cap. */
	bool			bo_accounted;
	u64			gart_addr;
	u64			*wptr;		/* firmware producer, num_regions */
	u64			*rptr;		/* SDK consumer, num_regions */
	u32			buffer_size;	/* records region bytes */
	u32			num_regions;
	u32			region_record_count;
	u64			bo_size;
	u64			wptr_offset;
	u64			rptr_offset;
	/*
	 * Caps live RAW_MMAP mappings at one. Each mapping holds its own stream
	 * kref, so destroy() (which frees the BO) cannot run while any is live.
	 */
	unsigned int		mmap_refs;

	u64			status;
	u64			target_exit_count;

	struct kref		refcount;
	struct mutex		lock;
};

/*========================= geometry / validation =======================*/

/*
 * Validate the requested records-region size and compute the BO layout:
 *   records[buffer_size] | wptr[num_regions] | rptr[num_regions] | pad
 */
static int kfd_dlog_stream_compute_layout(struct kfd_dlog_stream *st,
					  u32 buffer_size, u32 num_regions)
{
	u64 region_bytes = (u64)num_regions * KFD_DISPATCH_LOG_FW_RECORD_BYTES;
	u64 arr_bytes = (u64)num_regions * sizeof(u64);
	u32 per_region_records;
	u64 rptr_off, tail;

	if (!num_regions || !buffer_size)
		return -EINVAL;
	if (buffer_size > KFD_DISPATCH_LOG_MAX_BUFFER_SIZE)
		return -EINVAL;

	/* Must split into equal, power-of-two per-region rings. */
	if (buffer_size % region_bytes)
		return -EINVAL;

	per_region_records = buffer_size / (u32)region_bytes;
	if (!is_power_of_2(per_region_records))
		return -EINVAL;

	rptr_off = (u64)buffer_size + arr_bytes;
	tail = rptr_off + arr_bytes;

	st->buffer_size = buffer_size;
	st->num_regions = num_regions;
	st->region_record_count = per_region_records;
	st->wptr_offset = buffer_size;
	st->rptr_offset = rptr_off;
	st->bo_size = PAGE_ALIGN(tail);
	return 0;
}

/* Allocate the KFD-owned GTT BO (pinned+GART-bound+kmapped) and zero it. */
static int kfd_dlog_stream_alloc_bo(struct kfd_dlog_stream *st)
{
	struct amdgpu_device *adev;
	void *mem_obj = NULL;
	void *cpu_ptr = NULL;
	u64 gpu_addr = 0;
	int ret;

	if (!st->node)
		return -ENODEV;
	adev = st->node->adev;

	/* Bound aggregate pinned GTT before pinning; released on free. */
	if (!kfd_dlog_gtt_reserve(st->bo_size))
		return -ENOMEM;

	ret = amdgpu_amdkfd_alloc_kernel_mem(adev, st->bo_size,
					     AMDGPU_GEM_DOMAIN_GTT, &mem_obj,
					     &gpu_addr, &cpu_ptr, false, false);
	if (ret) {
		kfd_dlog_gtt_unreserve(st->bo_size);
		return ret;
	}
	st->bo_accounted = true;

	st->bo = mem_obj;
	st->gart_addr = gpu_addr;
	st->wptr = (u64 *)((char *)cpu_ptr + st->wptr_offset);
	st->rptr = (u64 *)((char *)cpu_ptr + st->rptr_offset);

	memset(cpu_ptr, 0, st->bo_size);
	/* Records/wptr must be zero before firmware (device DMA) is armed. */
	dma_wmb();

	return 0;
}

/*
 * Single detach. Runs under st->lock. It NULLs every raw external pointer --
 * @node, @wptr, @rptr -- and hands the owned BO out via *bo_out (NULLing
 * st->bo). After it returns no path may touch those pointers. Sets @torn_down
 * (read lock-free by mmap/poll/setup to reject a detached stream).
 *
 * The caller MUST free the BO (after dropping st->lock) and release GTT
 * accounting via kfd_dlog_stream_free_finish(). Only kfd_dlog_stream_destroy()
 * (the last kref put) runs this, so the BO is freed in exactly one place and
 * cannot double-free or race a second free. Target exit and GPU reset only
 * terminate/flag the stream; they do not detach.
 *
 * *bo_out and *unreserve are outputs the caller passes back to
 * kfd_dlog_stream_free_finish().
 */
static void kfd_dlog_stream_detach_locked(struct kfd_dlog_stream *st,
					  struct amdgpu_bo **bo_out,
					  bool *unreserve)
{
	lockdep_assert_held(&st->lock);

	st->torn_down = true;

	*bo_out = st->bo;
	*unreserve = st->bo_accounted;
	st->bo = NULL;
	st->bo_accounted = false;
	st->wptr = NULL;
	st->rptr = NULL;
	st->node = NULL;
}

/* Free the BO (outside st->lock) and release GTT accounting. */
static void kfd_dlog_stream_free_finish(struct amdgpu_bo *bo, bool unreserve,
					u64 bo_size)
{
	if (bo)
		amdgpu_amdkfd_free_kernel_mem(amdgpu_ttm_adev(bo->tbo.bdev),
					      (void **)&bo);
	if (unreserve)
		kfd_dlog_gtt_unreserve(bo_size);
}

/*=========================== stream lifecycle ==========================*/

void kfd_dlog_stream_get(struct kfd_dlog_stream *st)
{
	kref_get(&st->refcount);
}

/*
 * True once the stream is terminal (target exit): there is no live consumer, so
 * newly created queues must not be armed into it even if the PQM session was
 * retained after a failed unbind.
 */
bool kfd_dlog_stream_is_terminal(struct kfd_dlog_stream *st)
{
	return !st || READ_ONCE(st->terminal);
}

/*
 * Publish terminal state and disconnect the stream from its target. Idempotent.
 * This is NOT kfd_dlog_stream_detach_locked(): it does NOT free the KFD-owned BO
 * or NULL @node/@wptr/@rptr. A terminal (target-exit) stream keeps its BO
 * pinned+mapped so already-produced records remain readable through any live
 * mmap, with @node still valid until the BO-freeing detach_locked() runs at the
 * last kref put (kfd_dlog_stream_destroy()). It only clears st->target, sets
 * @terminal, and drops the process reference.
 *
 * @target_exited: this terminate is the target-process exit path. Latch
 * KFD_DLOG_STATUS_TARGET_EXITED and bump target_exit_count BEFORE clearing
 * st->target, so STATUS reports the exit on the normal exit path (not only via
 * the later pid_alive() probe, which cannot run once st->target is NULL).
 */
static void kfd_dlog_stream_terminate_flags(struct kfd_dlog_stream *st,
					    bool target_exited)
{
	struct kfd_process *target;

	mutex_lock(&st->lock);
	if (target_exited && !(st->status & KFD_DLOG_STATUS_TARGET_EXITED)) {
		st->target_exit_count++;
		st->status |= KFD_DLOG_STATUS_TARGET_EXITED;
	}
	target = st->target;
	st->target = NULL;
	WRITE_ONCE(st->terminal, true);
	mutex_unlock(&st->lock);

	/* Terminal is published above; the wake makes it visible to poll(). */
	wake_up_interruptible_poll(&st->poll_wq, DLOG_WAKE_MASK);

	if (target)
		kfd_unref_process(target);
}

static void kfd_dlog_stream_terminate(struct kfd_dlog_stream *st)
{
	kfd_dlog_stream_terminate_flags(st, false);
}

/*
 * Last put: detach + free the KFD-owned BO. Reached only after the PQM session
 * and every RAW_MMAP VMA (each held a kref) are gone, so no firmware can reach
 * the backing and no live mapping exists.
 */
static void kfd_dlog_stream_destroy(struct kref *kref)
{
	struct kfd_dlog_stream *st = container_of(kref, struct kfd_dlog_stream,
						  refcount);
	struct amdgpu_bo *bo;
	bool unreserve;

	kfd_dlog_stream_terminate(st);

	mutex_lock(&st->lock);
	kfd_dlog_stream_detach_locked(st, &bo, &unreserve);
	mutex_unlock(&st->lock);

	kfd_dlog_stream_free_finish(bo, unreserve, st->bo_size);

	mutex_destroy(&st->lock);
	kfree(st);
}

void kfd_dlog_stream_put(struct kfd_dlog_stream *st)
{
	kref_put(&st->refcount, kfd_dlog_stream_destroy);
}

static void kfd_dlog_stream_check_target_exit(struct kfd_dlog_stream *st)
{
	struct task_struct *lead;

	if (!st->target)
		return;

	lead = st->target->lead_thread;
	if (!lead || !pid_alive(lead)) {
		/* Latch: count target exit once, not once per STATUS call. */
		if (!(st->status & KFD_DLOG_STATUS_TARGET_EXITED))
			st->target_exit_count++;
		st->status |= KFD_DLOG_STATUS_TARGET_EXITED;
	}
}

/*========================= INFO / STATUS ioctls =========================*/

static void kfd_dlog_stream_fill_info(struct kfd_dlog_stream *st,
				      struct kfd_dlog_stream_info *info)
{
	memset(info, 0, sizeof(*info));
	info->abi_version = KFD_DLOG_STREAM_ABI_VERSION;
	info->fw_record_size = KFD_DISPATCH_LOG_FW_RECORD_BYTES;
	info->num_regions = st->num_regions;
	info->region_record_count = st->region_record_count;
	info->buffer_size = st->buffer_size;
	info->mmap_size = st->bo_size;
	/* Records region at BO offset 0: records | wptr[] | rptr[] | pad. */
	info->records_offset = 0;
	info->wptr_offset = st->wptr_offset;
	info->rptr_offset = st->rptr_offset;
	info->gpu_id = st->gpu_id;
	info->target_pid = st->target_pid;
	info->pasid = st->pasid;
	info->flags = 0;
}

static void kfd_dlog_stream_fill_status(struct kfd_dlog_stream *st,
					struct kfd_dlog_stream_status *status)
{
	kfd_dlog_stream_check_target_exit(st);

	memset(status, 0, sizeof(*status));
	status->status = st->status;
	status->target_exit_count = st->target_exit_count;
}

static long kfd_dlog_stream_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	struct kfd_dlog_stream *st = file->private_data;
	struct kfd_dlog_stream_args args;
	long ret = 0;

	if (cmd != KFD_DLOG_STREAM_IOC)
		return -ENOTTY;

	if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
		return -EFAULT;

	if (args.pad)
		return -EINVAL;

	/* Zero the union so unused bytes don't leak on the OUT copy. */
	memset(&args.info, 0,
	       sizeof(args) - offsetof(struct kfd_dlog_stream_args, info));

	switch (args.op) {
	case KFD_DLOG_STREAM_OP_INFO:
		/* Geometry is retained after terminal so a HUP reader can drain. */
		mutex_lock(&st->lock);
		if (!st->num_regions)
			ret = -EINVAL;
		else
			kfd_dlog_stream_fill_info(st, &args.info);
		mutex_unlock(&st->lock);
		break;
	case KFD_DLOG_STREAM_OP_STATUS:
		mutex_lock(&st->lock);
		kfd_dlog_stream_fill_status(st, &args.status);
		mutex_unlock(&st->lock);
		break;
	default:
		return -EINVAL;
	}

	if (!ret && copy_to_user((void __user *)arg, &args, sizeof(args)))
		ret = -EFAULT;

	return ret;
}

/*============ RAW_MMAP mode: VMA refcount + remap_pfn ============*/

static void kfd_dlog_stream_vma_open(struct vm_area_struct *vma)
{
	struct kfd_dlog_stream *st = vma->vm_private_data;

	kref_get(&st->refcount);
	mutex_lock(&st->lock);
	st->mmap_refs++;
	mutex_unlock(&st->lock);
}

static void kfd_dlog_stream_vma_close(struct vm_area_struct *vma)
{
	struct kfd_dlog_stream *st = vma->vm_private_data;

	mutex_lock(&st->lock);
	if (st->mmap_refs)
		st->mmap_refs--;
	mutex_unlock(&st->lock);

	kfd_dlog_stream_put(st);
}

static const struct vm_operations_struct kfd_dlog_stream_vm_ops = {
	.open	= kfd_dlog_stream_vma_open,
	.close	= kfd_dlog_stream_vma_close,
};

static int kfd_dlog_stream_mmap_pages(struct kfd_dlog_stream *st,
				      struct vm_area_struct *vma)
{
	struct ttm_buffer_object *bo = &st->bo->tbo;
	struct ttm_tt *ttm = bo->ttm;
	unsigned long start = vma->vm_start;
	unsigned long num_pages = st->bo_size >> PAGE_SHIFT;
	unsigned long i;
	int ret;

	/* BO is already pinned+GART-bound in GTT; just map its pages. */
	if (!ttm || !ttm->pages)
		return -EFAULT;
	if (num_pages > ttm->num_pages)
		return -EINVAL;

	vm_flags_set(vma, VM_PFNMAP);

	for (i = 0; i < num_pages; i++) {
		if (!ttm->pages[i])
			return -EFAULT;

		ret = remap_pfn_range(vma, start,
				      page_to_pfn(ttm->pages[i]),
				      PAGE_SIZE, vma->vm_page_prot);
		if (ret)
			return ret;
		start += PAGE_SIZE;
	}

	return 0;
}

static int kfd_dlog_stream_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct kfd_dlog_stream *st = file->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;
	int ret;

	if (vma->vm_pgoff)
		return -EINVAL;
	if (size != st->bo_size)
		return -EINVAL;
	if (vma->vm_flags & VM_EXEC)
		return -EINVAL;
	/* RAW_MMAP writes rptr[] through this VMA; require shared read/write. */
	if (!(vma->vm_flags & VM_SHARED) || !(vma->vm_flags & VM_READ) ||
	    !(vma->vm_flags & VM_WRITE))
		return -EINVAL;

	mutex_lock(&st->lock);
	/* Reject once torn down so mmap_pages() never touches a freed BO. */
	if (st->torn_down || !st->node || !st->bo) {
		ret = -ENODEV;
		goto unlock;
	}
	if (st->mmap_refs) {
		ret = -EBUSY;
		goto unlock;
	}

	vm_flags_set(vma, VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP);

	ret = kfd_dlog_stream_mmap_pages(st, vma);
	if (ret)
		goto unlock;

	/*
	 * Install vm_ops and take the VMA ref only after remap succeeds (initial
	 * mmap does not call ->open()). The mapping's kref keeps the BO alive
	 * until every mapping is gone; VM_DONTCOPY prevents fork-created refs.
	 */
	vma->vm_ops = &kfd_dlog_stream_vm_ops;
	vma->vm_private_data = st;
	kref_get(&st->refcount);
	st->mmap_refs++;
unlock:
	mutex_unlock(&st->lock);
	return ret;
}

/*==================== File operations + fd creation =====================*/

/*
 * close(fd) with a live RAW_MMAP VMA does not run ->release() until the VMA's
 * file ref is dropped. Publish terminal here; the backing survives on the
 * fd/VMA refs. Do not add ->flush(): it runs on every dup'd-fd close.
 */
static int kfd_dlog_stream_release(struct inode *inode, struct file *file)
{
	struct kfd_dlog_stream *st = file->private_data;

	kfd_dlog_stream_terminate(st);
	kfd_dlog_stream_put(st);
	return 0;
}

/*=============================== poll() ================================*/

/*
 * Level check under st->lock: wptr != rptr means unconsumed records (overrun is
 * the reader's problem, not poll()'s). A terminated (target-exit) stream keeps
 * its BO+wptr/rptr, so poll() still reports EPOLLIN|EPOLLHUP for the final
 * drain. Once torn down (last put) wptr/rptr are NULL and there are no records
 * to report.
 *
 * RAW_MMAP userspace advances rptr[] through its own PFN mapping while this
 * kmap reads the same pages; the level check relies on cached-GTT coherence
 * between the two views.
 *
 * Ordering: firmware publishes wptr[] by DMA into the BO. The firmware DMA
 * write to wptr[] is ordered against our read only by a device read barrier.
 * Issue a dma_rmb() before sampling wptr[] so a poll observes the producer's
 * latest wptr publish rather than a stale cached value. This is a level check:
 * if the barrier still races a just-arrived record, the reader's next poll pass
 * (re-armed on the same waitqueue) observes it.
 */
static bool kfd_dlog_stream_has_records(struct kfd_dlog_stream *st)
{
	u32 i;

	lockdep_assert_held(&st->lock);

	/* torn_down NULLs wptr/rptr; a husk has no records to report. */
	if (st->torn_down || !st->wptr || !st->rptr)
		return false;

	/* Observe the firmware's DMA wptr publish before comparing. */
	dma_rmb();

	for (i = 0; i < st->num_regions; i++) {
		if (READ_ONCE(st->wptr[i]) != READ_ONCE(st->rptr[i]))
			return true;
	}
	return false;
}

static __poll_t kfd_dlog_stream_poll(struct file *file,
				     struct poll_table_struct *wait)
{
	struct kfd_dlog_stream *st = file->private_data;
	bool has_records;
	__poll_t mask = 0;

	/* Register on the waitqueue before the level check (poll contract). */
	poll_wait(file, &st->poll_wq, wait);

	if (READ_ONCE(st->terminal))
		mask |= EPOLLHUP;

	mutex_lock(&st->lock);
	has_records = kfd_dlog_stream_has_records(st);
	mutex_unlock(&st->lock);

	if (has_records)
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

static const struct file_operations kfd_dlog_stream_fops = {
	.owner		= THIS_MODULE,
	.poll		= kfd_dlog_stream_poll,
	.unlocked_ioctl	= kfd_dlog_stream_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
	.mmap		= kfd_dlog_stream_mmap,
	.release	= kfd_dlog_stream_release,
	.llseek		= noop_llseek,
};

/*
 * Set up the stream: allocate/zero the KFD-owned BO and prime rptr[] to wptr[]
 * (both zero on a fresh BO). Arming the target's queues is added in a later
 * commit, so a stream created here gets no records.
 *
 * The rptr prime runs under st->lock and rechecks st->torn_down, so a racing
 * detach that NULLs wptr/rptr/node is observed rather than dereferenced.
 */
static int kfd_dlog_stream_setup(struct kfd_dlog_stream *st)
{
	u32 i;
	int ret;

	if (!st->node)
		return -ENODEV;

	ret = kfd_dlog_stream_alloc_bo(st);
	if (ret)
		return ret;

	mutex_lock(&st->lock);
	if (st->torn_down || !st->wptr || !st->rptr) {
		mutex_unlock(&st->lock);
		return -ENODEV;
	}
	for (i = 0; i < st->num_regions; i++)
		WRITE_ONCE(st->rptr[i], READ_ONCE(st->wptr[i]));
	mutex_unlock(&st->lock);

	return 0;
}

int kfd_dlog_stream_create_file(struct kfd_ioctl_dlog_args *args,
				struct file **filep)
{
	struct file *stream_file;
	struct kfd_dlog_stream *st;
	struct kfd_process *target;
	struct kfd_process_device *pdd;
	struct kfd_node *node;
	struct pid *pid;
	u32 num_regions;
	int fd, ret;

	if (!args || !filep)
		return -EINVAL;
	if (!args->gpu_id)
		return -EINVAL;
	/* Only RAW_MMAP is defined; every other flag bit is reserved-zero. */
	if (args->flags & ~KFD_DLOG_OPEN_F_RAW_MMAP)
		return -EINVAL;
	if (!(args->flags & KFD_DLOG_OPEN_F_RAW_MMAP))
		return -EINVAL;

	*filep = NULL;

	pid = find_get_pid(args->target_pid);
	if (!pid)
		return -ESRCH;

	target = kfd_lookup_process_by_pid(pid);
	put_pid(pid);
	if (!target)
		return -ESRCH;

	pdd = kfd_process_device_data_by_id(target, args->gpu_id);
	if (!pdd || !pdd->dev) {
		ret = -EINVAL;
		goto err_target;
	}
	node = pdd->dev;

	/* Single arch gate: nonzero only for supported GC versions, no FW gating. */
	num_regions = kfd_dispatch_log_node_num_regions(node);
	if (!num_regions) {
		ret = -EOPNOTSUPP;
		goto err_target;
	}

	/* Fail-closed auth BEFORE pinning: an unauthorized caller must not pin. */
	ret = kfd_dispatch_log_target_check_auth(target, args->gpu_id, node);
	if (ret)
		goto err_target;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st) {
		ret = -ENOMEM;
		goto err_target;
	}

	st->target = target;
	st->gpu_id = args->gpu_id;
	/*
	 * Report the target's TGID (group leader) in the CALLER's pid namespace,
	 * matching how args->target_pid was resolved (find_get_pid()).
	 */
	st->target_pid = target->lead_thread ?
		task_tgid_nr_ns(target->lead_thread,
				task_active_pid_ns(current)) :
		args->target_pid;
	st->pasid = pdd->pasid;
	st->node = node;
	kref_init(&st->refcount);
	mutex_init(&st->lock);
	init_waitqueue_head(&st->poll_wq);

	ret = kfd_dlog_stream_compute_layout(st, args->buffer_size, num_regions);
	if (ret)
		goto err_stream;

	/* st owns the target reference from here; terminate() releases it. */
	ret = kfd_dlog_stream_setup(st);
	if (ret)
		goto err_stream;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		goto err_setup;
	}

	/*
	 * anon_inode_create_getfile() (not _getfile) gives a PRIVATE per-inode
	 * address_space, so RAW_MMAP mappings are not entangled with other fds.
	 */
	stream_file = anon_inode_create_getfile(KFD_DLOG_STREAM_NAME,
						&kfd_dlog_stream_fops, st,
						O_RDWR | O_CLOEXEC, NULL);
	if (IS_ERR(stream_file)) {
		ret = PTR_ERR(stream_file);
		goto err_fd;
	}

	args->stream_fd = fd;
	*filep = stream_file;
	return 0;

err_fd:
	put_unused_fd(fd);
err_setup:
	/* Set up: publish terminal, then free via kref. */
	kfd_dlog_stream_terminate(st);
	kfd_dlog_stream_put(st);
	return ret;

err_stream:
	/* Not yet set up: no session state, but st owns target. */
	kfd_dlog_stream_put(st);
	return ret;

err_target:
	kfd_unref_process(target);
	return ret;
}
