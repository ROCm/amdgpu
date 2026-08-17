/*
 * Copyright 2018 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#if !defined(_AMDKFD_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _KFD_TRACE_H_


#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#include "kfd_priv.h"
#include <linux/kfd_ioctl.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM amdkfd
#define TRACE_INCLUDE_FILE kfd_trace


TRACE_EVENT(kfd_map_memory_to_gpu_start,
	    TP_PROTO(struct kfd_process *p),
	    TP_ARGS(p),
	    TP_STRUCT__entry(
			    __field(unsigned int, pid)
			    ),
	    TP_fast_assign(
			   __entry->pid = p->lead_thread->pid;
			   ),
	    TP_printk("Process pid =%u", __entry->pid)
);


TRACE_EVENT(kfd_map_memory_to_gpu_end,
	    TP_PROTO(struct kfd_process *p, u32 array_size, char *pStatusMsg),
	    TP_ARGS(p, array_size, pStatusMsg),
	    TP_STRUCT__entry(
				__field(unsigned int, pid)
				__field(unsigned int, array_size)
				__string(pStatusMsg, pStatusMsg)
			    ),
	    TP_fast_assign(
			   __entry->pid = p->lead_thread->pid;
				__entry->array_size = array_size;
				__amdkcl_assign_str(pStatusMsg, pStatusMsg);
			   ),
	    TP_printk("Process pid = %u, array_size =	%u, StatusMsg=%s",
				__entry->pid,
				 __entry->array_size,
				 __get_str(pStatusMsg))
);


TRACE_EVENT(kfd_kgd2kfd_schedule_evict_and_restore_process,
	    TP_PROTO(struct kfd_process *p, u32 delay_jiffies),
	    TP_ARGS(p, delay_jiffies),
	    TP_STRUCT__entry(
				__field(unsigned int, pid)
				__field(unsigned int, delay_jiffies)
			    ),
	    TP_fast_assign(
			   __entry->pid = p->lead_thread->pid;
			   __entry->delay_jiffies = delay_jiffies;
			),
	    TP_printk("Process pid = %u, delay_jiffies = %u",
		      __entry->pid,
		      __entry->delay_jiffies)
);


TRACE_EVENT(kfd_evict_process_worker_start,
	    TP_PROTO(struct kfd_process *p),
	    TP_ARGS(p),
	    TP_STRUCT__entry(
				__field(unsigned int, pid)
			    ),
	    TP_fast_assign(
			   __entry->pid = p->lead_thread->pid;
			   ),
	    TP_printk("Process pid=%u", __entry->pid)
);


TRACE_EVENT(kfd_evict_process_worker_end,
	    TP_PROTO(struct kfd_process *p, char *pStatusMsg),
	    TP_ARGS(p, pStatusMsg),
	    TP_STRUCT__entry(
			    __field(unsigned int, pid)
			    __string(pStatusMsg, pStatusMsg)
			    ),
	    TP_fast_assign(
			    __entry->pid = p->lead_thread->pid;
			    __amdkcl_assign_str(pStatusMsg, pStatusMsg);
			   ),
	    TP_printk("Process pid=%u, StatusMsg=%s",
			    __entry->pid, __get_str(pStatusMsg))
);


TRACE_EVENT(kfd_restore_process_worker_start,
	    TP_PROTO(struct kfd_process *p),
	    TP_ARGS(p),
	    TP_STRUCT__entry(
				__field(unsigned int, pid)
			    ),
	    TP_fast_assign(
			   __entry->pid = p->lead_thread->pid;
			   ),
	    TP_printk("Process pid=%u", __entry->pid)
);

TRACE_EVENT(kfd_restore_process_worker_end,
	    TP_PROTO(struct kfd_process *p, char *pStatusMsg),
	    TP_ARGS(p, pStatusMsg),
	    TP_STRUCT__entry(
				__field(unsigned int, pid)
				__string(pStatusMsg, pStatusMsg)
			    ),
	    TP_fast_assign(
				 entry->pid = p->lead_thread->pid;
				__amdkcl_assign_str(pStatusMsg, pStatusMsg);
			   ),
	    TP_printk("Process pid=%u, StatusMsg=%s",
			    __entry->pid, __get_str(pStatusMsg))
);

TRACE_EVENT(kfd_dlog_notify_interrupt,
	    TP_PROTO(u32 node_id, u32 source_id, u32 client_id, u32 pasid,
		     u32 vmid, u32 pipe_id, u32 context_id0, u32 matched),
	    TP_ARGS(node_id, source_id, client_id, pasid, vmid, pipe_id,
		    context_id0, matched),
	    TP_STRUCT__entry(
			    __field(u32, node_id)
			    __field(u32, source_id)
			    __field(u32, client_id)
			    __field(u32, pasid)
			    __field(u32, vmid)
			    __field(u32, pipe_id)
			    __field(u32, context_id0)
			    __field(u32, matched)
			    ),
	    TP_fast_assign(
			   __entry->node_id = node_id;
			   __entry->source_id = source_id;
			   __entry->client_id = client_id;
			   __entry->pasid = pasid;
			   __entry->vmid = vmid;
			   __entry->pipe_id = pipe_id;
			   __entry->context_id0 = context_id0;
			   __entry->matched = matched;
			   ),
	    TP_printk("node=%u src=%u client=0x%x pasid=0x%x vmid=%u pipe=%u ctx0=0x%08x matched=%u",
		      __entry->node_id, __entry->source_id, __entry->client_id,
		      __entry->pasid, __entry->vmid, __entry->pipe_id,
		      __entry->context_id0, __entry->matched)
);

/*
 * Emitted once per stream woken by a dispatch-log notify, so a wake can be
 * attributed to a specific stream rather than only counted.
 */
TRACE_EVENT(kfd_dlog_notify_stream,
	    TP_PROTO(u32 node_id, u32 pasid, u32 target_pid, u32 gpu_id),
	    TP_ARGS(node_id, pasid, target_pid, gpu_id),
	    TP_STRUCT__entry(
			    __field(u32, node_id)
			    __field(u32, pasid)
			    __field(u32, target_pid)
			    __field(u32, gpu_id)
			    ),
	    TP_fast_assign(
			   __entry->node_id = node_id;
			   __entry->pasid = pasid;
			   __entry->target_pid = target_pid;
			   __entry->gpu_id = gpu_id;
			   ),
	    TP_printk("node=%u pasid=0x%x target_pid=%u gpu_id=%u",
		      __entry->node_id, __entry->pasid, __entry->target_pid,
		      __entry->gpu_id)
);

#endif

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>
