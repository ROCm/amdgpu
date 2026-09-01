/* SPDX-License-Identifier: MIT */
#ifndef _KCL_HEADER_LINUX_FIND_H
#define _KCL_HEADER_LINUX_FIND_H

#ifdef HAVE_LINUX_FIND_H
#include_next <linux/find.h>
#else
#include <linux/bitmap.h>
#endif

#endif
