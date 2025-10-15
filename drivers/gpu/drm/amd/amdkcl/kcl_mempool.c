#include <kcl/kcl_mempool.h>
#include <linux/kmemleak.h>

#ifndef HAVE_MEMPOOL_ALLOC_PREALLOCATED
/*
 * Backport of mempool_alloc_preallocated from commit 37dcc69ad17a0
 * (mempool: introduce mempool_use_prealloc_only).
 */
static void *remove_element(mempool_t *pool)
{
	void *element = pool->elements[--pool->curr_nr];

	BUG_ON(pool->curr_nr < 0);
	return element;
}

void *kcl_mempool_alloc_preallocated(mempool_t *pool)
{
    void *element;
    unsigned long flags;

    spin_lock_irqsave(&pool->lock, flags);
    if (likely(pool->curr_nr)) {
        element = remove_element(pool);
        spin_unlock_irqrestore(&pool->lock, flags);
        /* paired with rmb in mempool_free(), read comment there */
        smp_wmb();
        /*
         * Update the allocation stack trace as this is more useful
         * for debugging.
         */
        kmemleak_update_trace(element);
        return element;
    }
    spin_unlock_irqrestore(&pool->lock, flags);

    return NULL;
}
EXPORT_SYMBOL_GPL(kcl_mempool_alloc_preallocated);
#endif
