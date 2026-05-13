/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Written by Mark Hemment, 1996 (markhe@nextd.demon.co.uk).
 *
 * (C) SGI 2006, Christoph Lameter
 *      Cleaned up and restructured to ease the addition of alternative
 *      implementations of SLAB allocators.
 * (C) Linux Foundation 2008-2013
 *      Unified interface for all slab allocators
 */
#ifndef AMDKCL_SLAB_H
#define AMDKCL_SLAB_H

#include <linux/gfp.h>
#include <linux/slab.h>
#include <kcl/kcl_cleanup.h>

#ifndef HAVE_KREALLOC_ARRAY
/**
 * krealloc_array - reallocate memory for an array.
 * @p: pointer to the memory chunk to reallocate
 * @new_n: new number of elements to alloc
 * @new_size: new size of a single member of the array
 * @flags: the type of memory to allocate (see kmalloc)
 */
static __must_check inline void *
krealloc_array(void *p, size_t new_n, size_t new_size, gfp_t flags)
{
        size_t bytes;

        if (unlikely(check_mul_overflow(new_n, new_size, &bytes)))
                return NULL;

        return krealloc(p, bytes, flags);
}
#endif

#ifndef HAVE_KMALLOC_SIZE_ROUNDUP
size_t kmalloc_size_roundup(size_t size);
#endif

#ifndef HAVE_LINUX_CLEANUP_H
DEFINE_FREE(kfree, void *, if (_T) kfree(_T))
#endif

#ifndef HAVE_KMALLOC_OBJ
/*
 * Backport typed allocation macros for old kernels.
 * These macros infer the type and size from the variable/type passed in.
 */

/* default_gfp: if no GFP flags provided, default to GFP_KERNEL */
#ifndef default_gfp
#define __kcl_default_gfp(a, b, ...) b
#define default_gfp(...) __kcl_default_gfp(,##__VA_ARGS__, GFP_KERNEL)
#endif

#define kmalloc_obj(VAR_OR_TYPE, ...) \
	((typeof(VAR_OR_TYPE) *)kmalloc(sizeof(typeof(VAR_OR_TYPE)), \
					default_gfp(__VA_ARGS__)))

#define kmalloc_objs(VAR_OR_TYPE, COUNT, ...) \
	((typeof(VAR_OR_TYPE) *)kmalloc(sizeof(typeof(VAR_OR_TYPE)) * (COUNT), \
					default_gfp(__VA_ARGS__)))

#define kzalloc_obj(P, ...) \
	((typeof(P) *)kzalloc(sizeof(typeof(P)), default_gfp(__VA_ARGS__)))

#define kzalloc_objs(P, COUNT, ...) \
	((typeof(P) *)kzalloc(sizeof(typeof(P)) * (COUNT), \
			      default_gfp(__VA_ARGS__)))

#define kzalloc_flex(P, FAM, COUNT, ...) \
	((typeof(P) *)kzalloc(offsetof(typeof(P), FAM) + \
			      sizeof(((typeof(P) *)0)->FAM[0]) * (COUNT), \
			      default_gfp(__VA_ARGS__)))

#define kvmalloc_obj(P, ...) \
	((typeof(P) *)kvmalloc(sizeof(typeof(P)), default_gfp(__VA_ARGS__)))

#define kvmalloc_objs(P, COUNT, ...) \
	((typeof(P) *)kvmalloc(sizeof(typeof(P)) * (COUNT), \
			       default_gfp(__VA_ARGS__)))

#define kvzalloc_obj(P, ...) \
	((typeof(P) *)kvzalloc(sizeof(typeof(P)), default_gfp(__VA_ARGS__)))

#define kvzalloc_objs(P, COUNT, ...) \
	((typeof(P) *)kvzalloc(sizeof(typeof(P)) * (COUNT), \
			       default_gfp(__VA_ARGS__)))

#define kvzalloc_flex(P, FAM, COUNT, ...) \
	((typeof(P) *)kvzalloc(offsetof(typeof(P), FAM) + \
			       sizeof(((typeof(P) *)0)->FAM[0]) * (COUNT), \
			       default_gfp(__VA_ARGS__)))

#endif /* HAVE_KMALLOC_OBJ */

#endif
