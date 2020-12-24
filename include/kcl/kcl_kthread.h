/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_KTHREAD_H
#define AMDKCL_KTHREAD_H

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/mmu_context.h>

#ifndef HAVE_KTHREAD_USE_MM
static inline
void kthread_use_mm(struct mm_struct *mm)
{
	use_mm(mm);
}
static inline
void kthread_unuse_mm(struct mm_struct *mm)
{
	unuse_mm(mm);
}
#endif

#endif /* AMDKCL_KTHREAD_H */
