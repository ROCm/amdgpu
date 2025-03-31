/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_KCL_COMPILER_TYPES_H
#define _KCL_KCL_COMPILER_TYPES_H

#include <linux/compiler_types.h>

/*
 * __has_builtin is supported on gcc >= 10, clang >= 3 and icc >= 21.
 * In the meantime, to support gcc < 10, we implement __has_builtin
 * by hand.
 */
#ifndef __has_builtin
#define __has_builtin(x) (0)
#endif

#ifndef __annotated
#if __has_builtin(__builtin_has_attribute)
#define __annotated(var, attr)	__builtin_has_attribute(var, attr)
#else
#define __annotated(var, attr)	(false)
#endif
#endif

#endif /* _KCL_KCL_COMPILER_TYPES_H */