// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2007 Jens Axboe <jens.axboe@oracle.com>
 *
 * Scatterlist handling helpers.
 */
#include <kcl/kcl_dma_fence_unwrap.h>
#include <linux/sort.h>

#ifndef HAVE_DMA_FENCE_DEDUP_ARRAY
static int fence_cmp(const void *_a, const void *_b)
{
	struct dma_fence *a = *(struct dma_fence **)_a;
	struct dma_fence *b = *(struct dma_fence **)_b;

	if (a->context < b->context)
		return -1;
	else if (a->context > b->context)
		return 1;

	if (dma_fence_is_later(b, a))
		return 1;
	else if (dma_fence_is_later(a, b))
		return -1;

	return 0;
}

/**
 * dma_fence_dedup_array - Sort and deduplicate an array of dma_fence pointers
 * @fences:     Array of dma_fence pointers to be deduplicated
 * @num_fences: Number of entries in the @fences array
 *
 * Sorts the input array by context, then removes duplicate
 * fences with the same context, keeping only the most recent one.
 *
 * The array is modified in-place and unreferenced duplicate fences are released
 * via dma_fence_put(). The function returns the new number of fences after
 * deduplication.
 *
 * Return: Number of unique fences remaining in the array.
 */
int kcl_dma_fence_dedup_array(struct dma_fence **fences, int num_fences)
{
	int i, j;

	sort(fences, num_fences, sizeof(*fences), fence_cmp, NULL);

	/*
	 * Only keep the most recent fence for each context.
	 */
	j = 0;
	for (i = 1; i < num_fences; i++) {
		if (fences[i]->context == fences[j]->context)
			dma_fence_put(fences[i]);
		else
			fences[++j] = fences[i];
	}

	return ++j;
}
EXPORT_SYMBOL_GPL(kcl_dma_fence_dedup_array);

#endif