/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AMDKCL_FENCE_BACKPORT_H
#define AMDKCL_FENCE_BACKPORT_H
#include <kcl/kcl_fence.h>
#include <kcl/kcl_slab.h>

/*
 * commit v4.14-rc3-601-g5f72db59160c
 * dma-buf/fence: Sparse wants __rcu on the object itself
 */
#ifdef AMDKCL_FENCE_GET_RCU_SAFE
#define dma_fence_get_rcu_safe _kcl_fence_get_rcu_safe
#endif
#endif
