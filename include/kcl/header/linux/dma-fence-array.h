/* SPDX-License-Identifier: MIT */
#ifndef _KCL_HEADER__LINUX_DMA_FENCE_ARRAY_H_H_
#define _KCL_HEADER__LINUX_DMA_FENCE_ARRAY_H_H_

#if !defined(HAVE_LINUX_FENCE_ARRAY_H)
#include_next <linux/dma-fence-array.h>
#else
#include_next <linux/fence-array.h>
#endif

#ifdef HAVE_DMA_FENCE_ARRAY_CREATE_5ARGS
/*
 * Old kernels have signal_on_any as the 5th parameter.
 * New code calls dma_fence_array_create() with 4 args.
 * Provide a wrapper to add false as the 5th argument.
 */
static inline struct dma_fence_array *
kcl_dma_fence_array_create(int num_fences, struct dma_fence **fences,
			   u64 context, unsigned seqno)
{
	return (dma_fence_array_create)(num_fences, fences, context, seqno, false);
}
#define dma_fence_array_create(num_fences, fences, context, seqno) \
	kcl_dma_fence_array_create(num_fences, fences, context, seqno)
#endif

#endif
