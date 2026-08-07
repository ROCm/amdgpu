/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_FOLIO_BACKPORT_H
#define _KCL_FOLIO_BACKPORT_H

#ifndef HAVE_SHMEM_READ_FOLIO

/*
 * On kernels without the folio API, map folio_* onto the equivalent struct
 * page helpers. These macros must be defined *before* including ttm_backup.h
 * so that its prototypes and the ttm_backup.c definitions agree on the type
 * (struct folio -> struct page).
 */
#define folio page
#define page_folio(p) (p)
#define folio_page(p, n) ((p) + (n))
#define folio_next_index(p) ((p)->index + 1)
#define folio_file_page(p, i) p
#define shmem_read_folio shmem_read_mapping_page
#define shmem_read_folio_gfp shmem_read_mapping_page_gfp
#define folio_lock lock_page
#define folio_unlock unlock_page
#define folio_mark_accessed mark_page_accessed
#define folio_mark_dirty set_page_dirty
#define folio_mapped page_mapcount
#define folio_clear_dirty_for_io clear_page_dirty_for_io
#define folio_set_reclaim SetPageReclaim
#define folio_clear_reclaim ClearPageReclaim
#define folio_test_writeback PageWriteback
#define folio_put put_page
#endif

#include <drm/ttm/ttm_backup.h>

#endif
