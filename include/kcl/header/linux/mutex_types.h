/* SPDX-License-Identifier: MIT */
#ifndef _KCL_HEADER_LINUX_MUTEX_TYPES_H
#define _KCL_HEADER_LINUX_MUTEX_TYPES_H

#ifdef HAVE_LINUX_MUTEX_TYPES_H
#include_next <linux/mutex_types.h>
#else
#include <linux/mutex.h>
#endif

#endif
