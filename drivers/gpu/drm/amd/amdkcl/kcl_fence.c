// SPDX-License-Identifier: GPL-2.0-only
/*
 * Fence mechanism for dma-buf and to allow for asynchronous dma access
 *
 * Copyright (C) 2012 Canonical Ltd
 * Copyright (C) 2012 Texas Instruments
 *
 * Authors:
 * Rob Clark <robdclark@gmail.com>
 * Maarten Lankhorst <maarten.lankhorst@canonical.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#include <linux/slab.h>
#include <kcl/kcl_fence.h>

#define CREATE_TRACE_POINTS
#include "kcl_fence_trace.h"

#ifdef AMDKCL_DMA_FENCE_OPS_ENABLE_SIGNALING
bool _kcl_fence_enable_signaling(struct dma_fence *f)
{
	return true;
}
EXPORT_SYMBOL(_kcl_fence_enable_signaling);
#endif

#if !defined(HAVE_DMA_FENCE_DESCRIBE)
/**
 * dma_fence_describe - Dump fence describtion into seq_file
 * @fence: the 6fence to describe
 * @seq: the seq_file to put the textual description into
 *
 * Dump a textual description of the fence and it's state into the seq_file.
 */
void dma_fence_describe(struct dma_fence *fence, struct seq_file *seq)
{
        seq_printf(seq, "%s %s seq %llu %ssignalled\n",
                   fence->ops->get_driver_name(fence),
                   fence->ops->get_timeline_name(fence), fence->seqno,
                   dma_fence_is_signaled(fence) ? "" : "un");
}
EXPORT_SYMBOL(dma_fence_describe);
#endif
