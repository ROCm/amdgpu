/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_LIST_LRU_H
#define AMDKCL_LIST_LRU_H

/*
 * On older kernels (e.g. 5.4) <linux/list_lru.h> and the <linux/shrinker.h>
 * it pulls in rely on the includer for spinlock_t/atomic_long_t and the
 * spin_lock() helpers, so include the prerequisites first.
 */
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/list_lru.h>

/*
 * v6.8 0a97c01cd20bb ("list_lru: allow explicit memcg and NUMA node
 * selection") changed list_lru_add() from 2 args to 4 args and added the
 * 2-arg wrapper list_lru_add_obj().
 *
 * v6.13 fb56fdf8b9a2f ("mm/list_lru: split the lock to per-cgroup scope")
 * later moved the per-sublist lock from struct list_lru_node into struct
 * list_lru_one and made struct list_lru_node.nr_items atomic_long_t.
 *
 * Some kernels (e.g. RHEL 10.2 based on v6.12) ship the 4-arg list_lru_add()
 * in headers but do NOT export the symbol, while list_lru_add_obj() is
 * exported. list_lru_add_obj() derives the NUMA node via
 * page_to_nid(virt_to_page(item)). That is invalid when @item is the lru
 * field of a struct page (a vmemmap address), as used by the TTM page pool,
 * and dereferencing the bogus page faults -> kernel oops.
 *
 * If the kernel genuinely exports the 4-arg list_lru_add()
 * (HAVE_4ARGS_LIST_LRU_ADD, detected from Module.symvers only), we use it
 * directly. Otherwise (RHEL 10.2 ships it but does not export it, or old 2-arg
 * kernels) we route list_lru_add() through our function pointer, which
 * amdkcl_list_lru_init() resolves to the kernel's own list_lru_add() via
 * kallsyms (works even when un-exported) or to our reimplementation in
 * kcl_list_lru.c. The TTM page pool uses a non-memcg-aware list_lru and always
 * passes the caller-provided @nid (never virt_to_page).
 *
 * Note: HAVE_4ARGS_LIST_LRU_ADD must be detected from Module.symvers only.
 * RHEL 10.2 keeps EXPORT_SYMBOL_GPL(list_lru_add) in source but does not export
 * the symbol; a source-grep export check would false-positive and leave amdttm
 * with an undefined list_lru_add at modpost.
 */
#ifndef HAVE_4ARGS_LIST_LRU_ADD
extern bool (*_kcl_list_lru_add)(struct list_lru *lru, struct list_head *item,
				 int nid, struct mem_cgroup *memcg);

/* Drop-in for the upstream 4-arg list_lru_add(lru, item, nid, memcg). */
#define list_lru_add(lru, item, nid, memcg) \
	_kcl_list_lru_add((lru), (item), (nid), (memcg))
#endif /* !HAVE_4ARGS_LIST_LRU_ADD */

#endif /* AMDKCL_LIST_LRU_H */
