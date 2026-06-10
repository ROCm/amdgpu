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
#include <kcl/kcl_drm_print.h>
#include <kcl/kcl_bitops.h>
#include <linux/stdarg.h>
#include <linux/slab.h>
#include <linux/string.h>

#if !defined(HAVE_DRM_ERR_MACRO)
void kcl_drm_err(const char *format, ...)
{
        struct va_format vaf;
        va_list args;

        va_start(args, format);
        vaf.fmt = format;
        vaf.va = &args;

        printk(KERN_ERR "[" DRM_NAME ":%ps] *ERROR* %pV",
               __builtin_return_address(0), &vaf);

        va_end(args);
}
EXPORT_SYMBOL(kcl_drm_err);

#endif

#ifndef HAVE_DRM_PRINT_BITS
/* Copied from v5.3-rc1-684-g141f6357f45c drivers/gpu/drm/drm_print.c */
void drm_print_bits(struct drm_printer *p, unsigned long value,
		    const char * const bits[], unsigned int nbits)
{
	bool first = true;
	unsigned int i;

	if (WARN_ON_ONCE(nbits > BITS_PER_TYPE(value)))
		nbits = BITS_PER_TYPE(value);

	for_each_set_bit(i, &value, nbits) {
		if (WARN_ON_ONCE(!bits[i]))
			continue;
		drm_printf(p, "%s%s", first ? "" : ",",
			   bits[i]);
		first = false;
	}
	if (first)
		drm_printf(p, "(none)");
}
EXPORT_SYMBOL(drm_print_bits);
#endif

/*
 * NULL-safe devcoredump printer.
 *
 * amdgpu's devcoredump code (amdgpu_devcoredump_format) runs the coredump
 * printer once with a NULL output buffer to compute the required size, then
 * a second time with a real buffer to print. This relies on the devcoredump
 * printer skipping writes when iterator->data == NULL, which was only added
 * upstream in v6.12 commit 53369581dc0c ("drm/printer: Allow NULL data in
 * devcoredump printer"). On older kernels (e.g. RHEL 9.x's drm.ko) the
 * in-kernel __drm_printfn_coredump unconditionally writes to iterator->data,
 * so the sizing pass dereferences NULL and crashes.
 *
 * That change only touched the function bodies in drm_print.c -- no symbol,
 * prototype or struct change -- so it is invisible to configure feature
 * tests. Always provide a NULL-safe copy here and route
 * drm_coredump_printer() through it (see kcl_drm_print.h /
 * backport/kcl_drm_print.h) so amdgpu never depends on the host drm.ko's
 * behaviour.
 *
 * Copied from v6.12 drivers/gpu/drm/drm_print.c.
 */
void _kcl_drm_puts_coredump(struct drm_printer *p, const char *str)
{
	struct drm_print_iterator *iterator = p->arg;
	ssize_t len;

	if (!iterator->remain)
		return;

	if (iterator->offset < iterator->start) {
		ssize_t copy;

		len = strlen(str);

		if (iterator->offset + len <= iterator->start) {
			iterator->offset += len;
			return;
		}

		copy = len - (iterator->start - iterator->offset);

		if (copy > iterator->remain)
			copy = iterator->remain;

		/* Copy out the bit of the string that we need */
		if (iterator->data)
			memcpy(iterator->data,
			       str + (iterator->start - iterator->offset), copy);

		iterator->offset = iterator->start + copy;
		iterator->remain -= copy;
	} else {
		ssize_t pos = iterator->offset - iterator->start;

		len = min_t(ssize_t, strlen(str), iterator->remain);

		if (iterator->data)
			memcpy(iterator->data + pos, str, len);

		iterator->offset += len;
		iterator->remain -= len;
	}
}
EXPORT_SYMBOL(_kcl_drm_puts_coredump);

void _kcl_drm_printfn_coredump(struct drm_printer *p, struct va_format *vaf)
{
	struct drm_print_iterator *iterator = p->arg;
	size_t len;
	char *buf;

	if (!iterator->remain)
		return;

	/* Figure out how big the string will be */
	len = snprintf(NULL, 0, "%pV", vaf);

	/* This is the easiest path, we've already advanced beyond the offset */
	if (iterator->offset + len <= iterator->start) {
		iterator->offset += len;
		return;
	}

	/* Then check if we can directly copy into the target buffer */
	if ((iterator->offset >= iterator->start) && (len < iterator->remain)) {
		ssize_t pos = iterator->offset - iterator->start;

		if (iterator->data)
			snprintf(((char *) iterator->data) + pos,
				 iterator->remain, "%pV", vaf);

		iterator->offset += len;
		iterator->remain -= len;

		return;
	}

	/*
	 * Finally, hit the slow path and make a temporary string to copy over
	 * using _kcl_drm_puts_coredump
	 */
	buf = kmalloc(len + 1, GFP_KERNEL | __GFP_NOWARN | __GFP_NORETRY);
	if (!buf)
		return;

	snprintf(buf, len + 1, "%pV", vaf);
	_kcl_drm_puts_coredump(p, (const char *) buf);

	kfree(buf);
}
EXPORT_SYMBOL(_kcl_drm_printfn_coredump);
