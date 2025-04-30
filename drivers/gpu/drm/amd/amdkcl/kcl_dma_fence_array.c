// SPDX-License-Identifier: GPL-2.0-only
/*
 * dma-fence-array: aggregate fences to be waited together
 *
 * Copyright (C) 2016 Collabora Ltd
 * Copyright (C) 2016 Advanced Micro Devices, Inc.
 * Authors:
 *	Gustavo Padovan <gustavo@padovan.org>
 *	Christian König <christian.koenig@amd.com>
 */

#ifndef HAVE_DMA_FENCE_ARRAY_FIRST
#include <kcl/kcl_dma_fence_array.h>

struct dma_fence *kcl_dma_fence_array_first(struct dma_fence *head)
{
	struct dma_fence_array *array;

	if (!head)
		return NULL;

	array = to_dma_fence_array(head);
	if (!array)
		return head;

	if (!array->num_fences)
		return NULL;

	return array->fences[0];
}
EXPORT_SYMBOL(kcl_dma_fence_array_first);

struct dma_fence *kcl_dma_fence_array_next(struct dma_fence *head,
				       unsigned int index)
{
	struct dma_fence_array *array = to_dma_fence_array(head);

	if (!array || index >= array->num_fences)
		return NULL;

	return array->fences[index];
}
EXPORT_SYMBOL(kcl_dma_fence_array_next);
#endif
