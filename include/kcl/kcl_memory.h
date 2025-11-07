/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_KCL_MEMORY_H
#define _KCL_KCL_MEMORY_H

#ifdef HAVE_NO_KVREALLOC
void *kvrealloc(const void *p, size_t oldsize, size_t newsize, gfp_t flags);
#endif

#endif
