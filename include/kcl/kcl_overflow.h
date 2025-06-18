#ifndef __KCL_LINUX_OVERFLOW_H
#define __KCL_LINUX_OVERFLOW_H

#include <linux/overflow.h>

#ifndef HAVE_SIZE_MUL
#define size_mul array_size
#endif
#endif
