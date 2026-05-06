/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KCL_SPINLOCK_H
#define __KCL_SPINLOCK_H

#include <linux/spinlock.h>
#include <kcl/kcl_cleanup.h>

#ifndef HAVE_LINUX_CLEANUP_H
DEFINE_LOCK_GUARD_1(spinlock_irqsave, spinlock_t,
		    spin_lock_irqsave(_T->lock, _T->flags),
		    spin_unlock_irqrestore(_T->lock, _T->flags),
		    unsigned long flags)
#endif /* HAVE_LINUX_CLEANUP_H */

#endif /* __KCL_SPINLOCK_H */
