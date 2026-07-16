/*
 * Copyright (C) 2016 Red Hat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Rob Clark <robdclark@gmail.com>
 */


// Copied from include/drm/drm_print.h
#ifndef _KCL_BACKPORT_KCL__DRM_PRINT_H__H_
#define _KCL_BACKPORT_KCL__DRM_PRINT_H__H_

#include <drm/drm_print.h>
#include <kcl/kcl_drm_print.h>

#if !defined(HAVE_DRM_PRINT_BITS_4ARGS) && \
	defined(HAVE_DRM_PRINT_BITS)
static inline
void _kcl_drm_print_bits(struct drm_printer *p, unsigned long value,
		    const char * const bits[], unsigned int nbits)
{
	unsigned int from, to;

	from = ffs(value);
	to = fls(value);
	WARN_ON_ONCE(to > nbits);

	drm_print_bits(p, value, bits, from, nbits);
}
#define drm_print_bits _kcl_drm_print_bits
#endif


#ifndef HAVE_DRM_DBG_PRINTER
static inline
struct drm_printer _kcl_drm_dbg_printer(struct drm_device *drm,
                                enum drm_debug_category category,
                                const char *prefix)
{
	return drm_debug_printer(prefix);
}
#define drm_dbg_printer _kcl_drm_dbg_printer
#endif

/*
 * On kernels < 6.13 redirect the native drm_coredump_printer() to the
 * NULL-safe KCL copy: the NULL-data guard amdgpu's sizing pass needs only
 * landed in v6.12 (commit 53369581dc0c), and we keep the copy through 6.12.x
 * to be conservative. On >= 6.13 use the native printer, so in-tree builds
 * (which don't compile amdkcl) don't pull in the _kcl_* symbols.
 */
#ifdef AMDKCL_DRM_COREDUMP_PRINTER_NEED_KCL
static inline struct drm_printer
_kcl_drm_coredump_printer(struct drm_print_iterator *iter)
{
	struct drm_printer p = {
		.printfn = _kcl_drm_printfn_coredump,
		.puts = _kcl_drm_puts_coredump,
		.arg = iter,
	};

	/* Set the internal offset of the iterator to zero */
	iter->offset = 0;

	return p;
}
#define drm_coredump_printer _kcl_drm_coredump_printer
#endif

#endif
