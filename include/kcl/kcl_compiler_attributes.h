/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_COMPILER_ATTRIBUTES_H
#define AMDKCL_COMPILER_ATTRIBUTES_H

#ifdef HAVE_LINUX_COMPILER_ATTRIBUTES_H
#include <linux/compiler_attributes.h>
#endif

#ifndef fallthrough
#define fallthrough                    do {} while (0)  /* fallthrough */
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __counted_by
#if __has_attribute(__counted_by__)
# define __counted_by(member)		__attribute__((__counted_by__(member)))
#else
# define __counted_by(member)
#endif
#endif

#ifndef  __cleanup
#ifdef __clang__
#define __cleanup(func) __maybe_unused __attribute__((__cleanup__(func)))
#else
#define __cleanup(func)			__attribute__((__cleanup__(func)))
#endif
#endif

#endif /* AMDKCL_COMPILER_ATTRIBUTES_H */
