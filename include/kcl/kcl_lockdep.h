/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_LOCKDEP_H
#define _KCL_LOCKDEP_H

#include <linux/lockdep.h>

/*
 * Some legacy kernels don't provide lockdep_assert_not_held().  It is only
 * used for debug assertions, so fall back to a no-op when unavailable.
 */
#ifndef lockdep_assert_not_held
#define lockdep_assert_not_held(l) do { (void)(l); } while (0)
#endif

#endif /* _KCL_LOCKDEP_H */