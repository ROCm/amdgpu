/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Mutexes: blocking mutual exclusion locks
 *
 * started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 * This file contains the main data structure and API definitions.
 */
#ifndef __KCL_MUTEX_H
#define __KCL_MUTEX_H

#include <linux/mutex.h>
#include <kcl/kcl_cleanup.h>

#ifndef HAVE_LINUX_CLEANUP_H
DEFINE_GUARD(mutex, struct mutex *, mutex_lock(_T), mutex_unlock(_T))
#endif

#endif /* __LINUX_MUTEX_H */
