/* SPDX-License-Identifier: MIT */
#ifndef KCL_BACKPORT_DMA_RESV_H
#define KCL_BACKPORT_DMA_RESV_H

#include <linux/dma-resv.h>

static inline void kcl_dma_resv_iter_begin(struct dma_resv_iter *cursor,
					   struct dma_resv *obj,
					   enum dma_resv_usage usage)
{
	dma_resv_iter_begin(cursor, obj, usage);

#if !defined(HAVE_DMA_RESV_FENCES)
	cursor->excl_fence = NULL;
	cursor->kernel_iter = NULL;
#endif
}

static inline void kcl_dma_resv_iter_end(struct dma_resv_iter *cursor)
{
	dma_resv_iter_end(cursor);

#if !defined(HAVE_DMA_RESV_FENCES)
	dma_fence_put(cursor->excl_fence);
	dma_fence_put(cursor->kernel_iter);
#endif
}

#define dma_resv_iter_begin kcl_dma_resv_iter_begin
#define dma_resv_iter_end kcl_dma_resv_iter_end

#endif
