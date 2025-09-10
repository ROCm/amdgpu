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

#ifndef HAVE_VMEMDUP_ARRAY_USER
#ifndef __realloc_size
#define __realloc_size(x, y)
#endif
/**
 * memdup_array_user - duplicate array from user space
 * @src: source address in user space
 * @n: number of array members to copy
 * @size: size of one array member
 *
 * Return: an ERR_PTR() on failure. Result is physically
 * contiguous, to be freed by kfree().
 */
static inline __realloc_size(2, 3)
void *kcl_memdup_array_user(const void __user *src, size_t n, size_t size)
{
	size_t nbytes;

	if (check_mul_overflow(n, size, &nbytes))
		return ERR_PTR(-EOVERFLOW);

	return memdup_user(src, nbytes);
}
#define memdup_array_user kcl_memdup_array_user

/**
 * vmemdup_array_user - duplicate array from user space
 * @src: source address in user space
 * @n: number of array members to copy
 * @size: size of one array member
 *
 * Return: an ERR_PTR() on failure. Result may be not
 * physically contiguous. Use kvfree() to free.
 */
static inline __realloc_size(2, 3)
void *kcl_vmemdup_array_user(const void __user *src, size_t n, size_t size)
{
	size_t nbytes;

	if (check_mul_overflow(n, size, &nbytes))
		return ERR_PTR(-EOVERFLOW);

	return vmemdup_user(src, nbytes);
}
#define vmemdup_array_user kcl_vmemdup_array_user
#endif  /* HAVE_VMEMDUP_ARRAY_USER */

#endif  /* _KCL_KCL_STRING_H */
