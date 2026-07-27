/* SPDX-License-Identifier: MIT */
#include <kcl/kcl_list_lru.h>
#include "kcl_common.h"
#if defined(CONFIG_X86_KERNEL_IBT)
#include <asm/cpufeature.h>
#endif

#ifndef HAVE_4ARGS_LIST_LRU_ADD
/*
 * Fallback reimplementation of the upstream 4-arg
 * list_lru_add(lru, item, nid, memcg) for the non-memcg-aware case used by the
 * TTM page pool (@memcg is always NULL). Only used if the kernel's own
 * list_lru_add() cannot be resolved through kallsyms.
 *
 * Mirrors mm/list_lru.c for this case:
 *  - the target sublist is always &nlru->lru (list_lru_from_memcg_idx()
 *    returns it for a non-memcg lru), so no lock_list_lru_of_memcg() lookup;
 *  - the nr_items == LONG_MIN check is skipped, it only guards memcg-aware
 *    sublists being reparented;
 *  - set_shrinker_bit() is skipped, it is a no-op for memcg == NULL and
 *    shrinker_id == -1 (list_lru_init()), and is not exported anyway.
 *
 * Never uses virt_to_page(item); @nid is provided by the caller.
 */
static bool _kcl_list_lru_add_stub(struct list_lru *lru, struct list_head *item,
				   int nid, struct mem_cgroup *memcg)
{
#ifdef HAVE_LIST_LRU_EXT
	/* Oracle UEK wraps node[] inside ->ext to preserve KABI. */
	struct list_lru_node *nlru = &lru->ext->node[nid];
#else
	struct list_lru_node *nlru = &lru->node[nid];
#endif
	struct list_lru_one *l = &nlru->lru;

	(void)memcg;

#ifdef HAVE_LIST_LRU_ONE_LOCK
	/* New layout: lock lives in list_lru_one, node->nr_items is atomic. */
	spin_lock(&l->lock);
	if (list_empty(item)) {
		list_add_tail(item, &l->list);
		l->nr_items++;
		spin_unlock(&l->lock);
		atomic_long_inc(&nlru->nr_items);
		return true;
	}
	spin_unlock(&l->lock);
#else
	/* Old layout: lock and plain nr_items both live in list_lru_node. */
	spin_lock(&nlru->lock);
	if (list_empty(item)) {
		list_add_tail(item, &l->list);
		l->nr_items++;
		nlru->nr_items++;
		spin_unlock(&nlru->lock);
		return true;
	}
	spin_unlock(&nlru->lock);
#endif
	return false;
}

bool (*_kcl_list_lru_add)(struct list_lru *lru, struct list_head *item,
			  int nid, struct mem_cgroup *memcg);
EXPORT_SYMBOL(_kcl_list_lru_add);
#endif /* !HAVE_4ARGS_LIST_LRU_ADD */

void amdkcl_list_lru_init(void)
{
#ifndef HAVE_4ARGS_LIST_LRU_ADD
#ifdef HAVE_LIST_LRU_ADD_OBJ
	/*
	 * list_lru_add_obj() exists, so the kernel's list_lru_add() is the
	 * 4-arg form (both were added in the same commit). Resolve the real
	 * symbol through kallsyms so we use the kernel's own implementation,
	 * even when it is present but not exported (e.g. RHEL 10.2); fall back
	 * to our stub only if the symbol cannot be found.
	 */
	_kcl_list_lru_add = amdkcl_fp_setup("list_lru_add", _kcl_list_lru_add_stub);
#if defined(CONFIG_X86_KERNEL_IBT)
	/*
	 * On IBT-enforcing Intel CPUs register_kprobe() (used inside
	 * amdkcl_fp_setup() when kallsyms_lookup_name is unavailable) hands back
	 * a pointer just past the function's ENDBR, and calling it indirectly
	 * raises a #CP "Missing ENDBR" fault. Rather than fix up the resolved
	 * address, just use our own stub, which is a correctly-entered function.
	 */
	if (cpu_feature_enabled(X86_FEATURE_IBT))
		_kcl_list_lru_add = _kcl_list_lru_add_stub;
#endif
#else
	/*
	 * No list_lru_add_obj() means the symbol named list_lru_add is the old
	 * 2-arg form, which would virt_to_page(item) internally. Do not resolve
	 * it; always use our 4-arg stub.
	 */
	_kcl_list_lru_add = _kcl_list_lru_add_stub;
#endif
#endif /* !HAVE_4ARGS_LIST_LRU_ADD */
}
