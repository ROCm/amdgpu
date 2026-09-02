/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_FDTABLE_H
#define _KCL_FDTABLE_H

#ifndef HAVE_KERNEL_CLOSE_FD
#include <linux/fdtable.h>
#include <linux/sched.h>

static inline int close_fd(unsigned int fd)
{
    return __close_fd(current->files, fd);
}
#endif

#endif
