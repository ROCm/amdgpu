/* SPDX-License-Identifier: MIT */
#ifndef AMDGPU_BACKPORT_KCL_MEMPOOL_H
#define AMDGPU_BACKPORT_KCL_MEMPOOL_H

#include <linux/mempool.h>

#ifndef HAVE_MEMPOOL_ALLOC_PREALLOCATED

extern void *kcl_mempool_alloc_preallocated(mempool_t *pool) __malloc;

#define mempool_alloc_preallocated kcl_mempool_alloc_preallocated

#endif

#endif
