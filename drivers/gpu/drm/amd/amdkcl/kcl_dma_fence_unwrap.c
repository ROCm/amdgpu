// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2007 Jens Axboe <jens.axboe@oracle.com>
 *
 * Scatterlist handling helpers.
 */
#include <kcl/kcl_dma_fence_unwrap.h>
#include <kcl/kcl_dma_fence.h>
#include <linux/dma-fence-array.h>
#include <linux/slab.h>
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

#ifndef HAVE_DMA_FENCE_UNWRAP_MERGE
/* Implementation for the dma_fence_unwrap_merge() macro, don't use directly */
struct dma_fence *kcl__dma_fence_unwrap_merge(unsigned int num_fences,
					      struct dma_fence **fences,
					      struct dma_fence_unwrap *iter)
{
	struct dma_fence *tmp, *unsignaled = NULL, **array;
	struct dma_fence_array *result;
	ktime_t timestamp;
	int i, count;

	count = 0;
	timestamp = ns_to_ktime(0);
	for (i = 0; i < num_fences; ++i) {
		dma_fence_unwrap_for_each(tmp, &iter[i], fences[i]) {
			if (!dma_fence_is_signaled(tmp)) {
				dma_fence_put(unsignaled);
				unsignaled = dma_fence_get(tmp);
				++count;
			} else {
				ktime_t t = dma_fence_timestamp(tmp);

				if (ktime_after(t, timestamp))
					timestamp = t;
			}
		}
	}

	/*
	 * If we couldn't find a pending fence just return a private signaled
	 * fence with the timestamp of the last signaled one.
	 *
	 * Or if there was a single unsignaled fence left we can return it
	 * directly and early since that is a major path on many workloads.
	 */
	if (count == 0)
		return dma_fence_get_stub();
	else if (count == 1)
		return unsignaled;

	dma_fence_put(unsignaled);

	array = kmalloc_array(count, sizeof(*array), GFP_KERNEL);
	if (!array)
		return NULL;

	count = 0;
	for (i = 0; i < num_fences; ++i) {
		dma_fence_unwrap_for_each(tmp, &iter[i], fences[i]) {
			if (!dma_fence_is_signaled(tmp)) {
				array[count++] = dma_fence_get(tmp);
			} else {
				ktime_t t = dma_fence_timestamp(tmp);

				if (ktime_after(t, timestamp))
					timestamp = t;
			}
		}
	}

	if (count == 0 || count == 1)
		goto return_fastpath;

	count = dma_fence_dedup_array(array, count);

	if (count > 1) {
		result = dma_fence_array_create(count, array,
						dma_fence_context_alloc(1),
						1, false);
		if (!result) {
			for (i = 0; i < count; i++)
				dma_fence_put(array[i]);
			tmp = NULL;
			goto return_tmp;
		}
		return &result->base;
	}

return_fastpath:
	if (count == 0)
		tmp = dma_fence_get_stub();
	else
		tmp = array[0];

return_tmp:
	kfree(array);
	return tmp;
}
EXPORT_SYMBOL_GPL(kcl__dma_fence_unwrap_merge);

#endif