/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KCL_BACKPORT_KCL_LIST_LRU_H__
#define __KCL_BACKPORT_KCL_LIST_LRU_H__

/*
 * kcl_list_lru.h pulls in <linux/list_lru.h> (and its atomic/spinlock
 * prerequisites) before we define the list_lru_add() macro below. This ordering
 * is required: the kernel's own "bool list_lru_add(...)" declaration must be
 * parsed first, otherwise the macro would mangle it. Because this header is
 * force-included through backport/backport.h ahead of the .c file's own
 * includes, priming the <linux/list_lru.h> include guard here keeps every later
 * inclusion a no-op, so only call sites are rewritten.
 */
#include <kcl/kcl_list_lru.h>

#ifndef HAVE_4ARGS_LIST_LRU_ADD
/* Drop-in for the upstream 4-arg list_lru_add(lru, item, nid, memcg). */
#define list_lru_add _kcl_list_lru_add
#endif /* !HAVE_4ARGS_LIST_LRU_ADD */

#endif /* __KCL_BACKPORT_KCL_LIST_LRU_H__ */
