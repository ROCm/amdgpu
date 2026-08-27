/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_KCL_SCHED_MM_H
#define _KCL_KCL_SCHED_MM_H

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/gfp.h>
#include <linux/capability.h>
#include <linux/rcupdate.h>
#include <linux/sched/coredump.h>
#ifdef HAVE_TASK_EXEC_STATE_DUMPABLE
#include <linux/sched/exec_state.h>
#endif

/*
 * kcl_task_ptrace_dumpable_denied - PTRACE dumpability gate for @task.
 *
 * Returns true when @task is non-dumpable and the caller lacks CAP_SYS_PTRACE
 * in the target's user namespace (i.e. access must be denied).
 *
 * Task dumpability moved from mm_struct (get_dumpable()/SUID_DUMP_USER and
 * mm->user_ns) into a per-task struct task_exec_state.
 */
static inline bool kcl_task_ptrace_dumpable_denied(struct task_struct *task,
						   struct mm_struct *mm)
{
#ifdef HAVE_TASK_EXEC_STATE_DUMPABLE
	struct task_exec_state *es;
	bool denied;

	rcu_read_lock();
	es = rcu_dereference(task->exec_state);
	denied = READ_ONCE(es->dumpable) != TASK_DUMPABLE_OWNER &&
		 !ns_capable(es->user_ns, CAP_SYS_PTRACE);
	rcu_read_unlock();

	return denied;
#else
	return get_dumpable(mm) != SUID_DUMP_USER &&
	       !ns_capable(mm->user_ns, CAP_SYS_PTRACE);
#endif
}

#ifndef SHRINK_EMPTY
#define SHRINK_EMPTY (~0UL - 1)
#define SHRINK_STOP (~0UL)
#endif

#ifndef HAVE_FS_RECLAIM_ACQUIRE
#ifdef CONFIG_LOCKDEP
extern void __fs_reclaim_acquire(void);
extern void __fs_reclaim_release(void);
static inline void fs_reclaim_acquire(gfp_t gfp_mask) {
	return _kcl_fs_reclaim_acquire(gfp_mask);
}
static inline void fs_reclaim_release(gfp_t gfp_mask) {
	return  _kcl_fs_reclaim_release(gfp_mask);
}
#else
static inline void __fs_reclaim_acquire(void) { }
static inline void __fs_reclaim_release(void) { }
static inline void fs_reclaim_acquire(gfp_t gfp_mask) { }
static inline void fs_reclaim_release(gfp_t gfp_mask) { }
#endif /* CONFIG_LOCKDEP */
#endif /* HAVE_FS_RECLAIM_ACQUIRE */

#ifndef HAVE_MEMALLOC_NORECLAIM_SAVE
static inline unsigned int memalloc_noreclaim_save(void)
{
	unsigned int flags = current->flags & PF_MEMALLOC;
	current->flags |= PF_MEMALLOC;
	return flags;
}

static inline void memalloc_noreclaim_restore(unsigned int flags)
{
	current->flags = (current->flags & ~PF_MEMALLOC) | flags;
}
#endif /* HAVE_MEMALLOC_NORECLAIM_SAVE */

#endif
