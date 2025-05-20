/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_FOLIO_BACKPORT_H
#define _KCL_FOLIO_BACKPORT_H
#include <drm/ttm/ttm_backup.h>

#ifndef HAVE_SHMEM_READ_FOLIO

#define folio page
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

#endif
