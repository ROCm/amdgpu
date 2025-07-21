/* SPDX-License-Identifier: GPL-2.0 */

#ifndef KCL_KCL_HIGHMEM_INTERNAL_H
#define KCL_KCL_HIGHMEM_INTERNAL_H

#include <linux/mm.h>
#include <linux/highmem.h>

#ifndef HAVE_KMAP_LOCAL

static inline void *kmap_local_page(struct page *page)
{
        return page_address(page);
}

static inline void *kmap_local_page_prot(struct page *page, pgprot_t prot)
{
        return kmap_local_page(page);
}

#endif

#ifndef kunmap_local

static inline void __kunmap_local(void *addr)
{
#ifdef ARCH_HAS_FLUSH_ON_KUNMAP
        kunmap_flush_on_unmap(addr);
#endif
}

#define kunmap_local(__addr)                                    \
do {                                                            \
        BUILD_BUG_ON(__same_type((__addr), struct page *));     \
        __kunmap_local(__addr);                                 \
} while (0)
#endif /* kunmap_local */

#ifdef CONFIG_HIGHMEM
#ifndef HAVE_KMAP_LOCAL_PAGE_TRY_FROM_PANIC
static inline void *kmap_local_page_try_from_panic(struct page *page)
{
	if (!PageHighMem(page))
		return page_address(page);
	/* If the page is in HighMem, it's not safe to kmap it.*/
	return NULL;
}
#endif
#else
#ifndef HAVE_KMAP_LOCAL_PAGE_TRY_FROM_PANIC
static inline void *kmap_local_page_try_from_panic(struct page *page)
{
	return page_address(page);
}
#endif
#endif

#endif
