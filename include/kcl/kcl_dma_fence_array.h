/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * fence-array: aggregates fence to be waited together
 *
 * Copyright (C) 2016 Collabora Ltd
 * Copyright (C) 2016 Advanced Micro Devices, Inc.
 * Authors:
 *	Gustavo Padovan <gustavo@padovan.org>
 *	Christian König <christian.koenig@amd.com>
 */

#ifndef __KCL_LINUX_DMA_FENCE_ARRAY_H
#define __KCL_LINUX_DMA_FENCE_ARRAY_H

#ifndef HAVE_DMA_FENCE_ARRAY_FIRST
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>

struct dma_fence *kcl_dma_fence_array_first(struct dma_fence *head);
#define dma_fence_array_first kcl_dma_fence_array_first

struct dma_fence *kcl_dma_fence_array_next(struct dma_fence *head,
				       unsigned int index);
#define dma_fence_array_next kcl_dma_fence_array_next

#endif
#endif
