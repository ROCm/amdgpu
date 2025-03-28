// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright 2025 Advanced Micro Devices, Inc.
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
 */
#include <rdma/peer_mem.h>

/* If there is no peerdirect support, set up KCL function stubs to avoid any compilation failures */
#ifndef HAVE_KFD_PEERDIRECT_SUPPORT

void* (*_kcl_ib_register_peer_memory_client)(struct peer_memory_client
					*peer_client,
					invalidate_peer_memory
					*invalidate_callback);
EXPORT_SYMBOL(_kcl_ib_register_peer_memory_client);

void (*_kcl_ib_unregister_peer_memory_client)(void *reg_handle);
EXPORT_SYMBOL(_kcl_ib_unregister_peer_memory_client);
#endif

void amdkcl_peerdirect_init(void)
{
#ifndef HAVE_KFD_PEERDIRECT_SUPPORT
	_kcl_ib_register_peer_memory_client = amdkcl_fp_setup("ib_register_peer_memory_client", NULL);
	_kcl_ib_unregister_peer_memory_client = amdkcl_fp_setup("ib_unregister_peer_memory_client", NULL);
#endif
}
