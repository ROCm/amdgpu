/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_LINUX_FILE_H
#define AMDKCL_LINUX_FILE_H

#include <linux/file.h>

#ifndef fd_file
#define fd_file(f) ((f).file)
#endif

#ifndef HAVE_FD_EMPTY
static inline bool fd_empty(struct fd f)
{
        return !fd_file(f);
}
#endif

#ifndef HAVE_LINUX_CLEANUP_H
DEFINE_CLASS(fd, struct fd, fdput(_T), fdget(fd), int fd)
#endif

#endif
