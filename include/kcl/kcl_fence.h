/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Fence mechanism for dma-buf to allow for asynchronous dma access
 *
 * Copyright (C) 2012 Canonical Ltd
 * Copyright (C) 2012 Texas Instruments
 *
 * Authors:
 * Rob Clark <robdclark@gmail.com>
 * Maarten Lankhorst <maarten.lankhorst@canonical.com>
 */
#ifndef AMDKCL_FENCE_H
#define AMDKCL_FENCE_H

#include <linux/version.h>
#include <kcl/kcl_rcupdate.h>
#include <linux/dma-fence.h>

#if !defined(HAVE__DMA_FENCE_IS_LATER_2ARGS)

#if !defined(HAVE_DMA_FENCE_OPS_USE_64BIT_SEQNO)
static inline bool __dma_fence_is_later(u64 f1, u64 f2)
{
	
	/* This is for backward compatibility with drivers which can only handle
	 * 32bit sequence numbers. Use a 64bit compare when any of the higher
	 * bits are none zero, otherwise use a 32bit compare with wrap around
	 * handling.
	 */
	if (upper_32_bits(f1) || upper_32_bits(f2))
		return f1 > f2;

	return (int)(lower_32_bits(f1) - lower_32_bits(f2)) > 0;
}

#elif !defined(HAVE__DMA_FENCE_IS_LATER_WITH_OPS_ARG) && \
	defined(HAVE_DMA_FENCE_OPS_USE_64BIT_SEQNO)
static inline bool __dma_fence_is_later(u64 f1, u64 f2,
                                        const struct dma_fence_ops *ops)
{
        /* This is for backward compatibility with drivers which can only handle
         * 32bit sequence numbers. Use a 64bit compare when the driver says to
         * do so.
         */
        if (ops->use_64bit_seqno)
                return f1 > f2;

        return (int)(lower_32_bits(f1) - lower_32_bits(f2)) > 0;
}

#endif
#endif /* HAVE__DMA_FENCE_IS_LATER_2ARGS */

/*
 * commit v4.18-rc2-519-gc701317a3eb8
 * dma-fence: Make ->enable_signaling optional
 */
#if DRM_VERSION_CODE < DRM_VERSION(4, 19, 0)
#define AMDKCL_DMA_FENCE_OPS_ENABLE_SIGNALING
bool _kcl_fence_enable_signaling(struct dma_fence *f);
#define AMDKCL_DMA_FENCE_OPS_ENABLE_SIGNALING_OPTIONAL \
	.enable_signaling = _kcl_fence_enable_signaling,
#else
#define AMDKCL_DMA_FENCE_OPS_ENABLE_SIGNALING_OPTIONAL
#endif

#if !defined(HAVE_DMA_FENCE_DESCRIBE)
void dma_fence_describe(struct dma_fence *fence, struct seq_file *seq);
#endif

#endif /* AMDKCL_FENCE_H */
