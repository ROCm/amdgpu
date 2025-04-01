/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_KCL_STRING_H
#define _KCL_KCL_STRING_H

#include <linux/string.h>
#include <kcl/kcl_fortify_string.h>
#include <kcl/kcl_args.h>
#include <kcl/kcl_compiler.h>

#ifndef HAVE_STRSCPY_ALLOW_TWO_ARGUMENTS
extern ssize_t real_kcl_strscpy(char *, const char *, size_t) __RENAME(strscpy);

/*
 * The 2 argument style can only be used when dst is an array with a
 * known size.
 */
#define kcl_strscpy0(dst, src, ...)	\
	real_kcl_strscpy(dst, src, sizeof(dst) + __must_be_array(dst) +	\
	__must_be_cstr(dst) + __must_be_cstr(src))
#define kcl_strscpy1(dst, src, size)	\
	real_kcl_strscpy(dst, src, size + __must_be_cstr(dst) + __must_be_cstr(src))

#define strscpy(dst, src, ...)	\
	CONCATENATE(kcl_strscpy, COUNT_ARGS(__VA_ARGS__))(dst, src, __VA_ARGS__)
#endif

#endif  /* _KCL_KCL_STRING_H */
