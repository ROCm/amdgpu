/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __KCL_DRM_PRIME_H__
#define __KCL_DRM_PRIME_H__

#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/scatterlist.h>

#ifndef HAVE_DRM_PRIME_SG_TO_DMA_ADDR_ARRAY
static inline
int drm_prime_sg_to_dma_addr_array(struct sg_table *sgt, dma_addr_t *addrs,
				   int max_entries)
{
	struct sg_dma_page_iter dma_iter;
	dma_addr_t *a = addrs;

	for_each_sg_dma_page(sgt->sgl, &dma_iter, sgt->nents, 0) {
		if (WARN_ON(a - addrs >= max_entries))
			return -1;

		*a++ = sg_page_iter_dma_address(&dma_iter);
	}

	return 0;
}
#endif /* HAVE_DRM_PRIME_SG_TO_DMA_ADDR_ARRAY */
#endif
