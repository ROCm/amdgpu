/* SPDX-License-Identifier: MIT */
#ifndef AMDGPU_BACKPORT_H
#define AMDGPU_BACKPORT_H

#include <linux/version.h>
#include <linux/kernel.h>
#include <kcl/kcl_moduleparam.h>
#include <kcl/kcl_idr.h>
#include <kcl/kcl_dma_mapping.h>
#include <kcl/kcl_i2c.h>
#include <kcl/kcl_video.h>
#include <kcl/kcl_acpi.h>
#include <kcl/kcl_kthread.h>
#include <kcl/kcl_backlight.h>
#include <kcl/kcl_fs.h>
#include <kcl/kcl_list.h>
#include <kcl/backport/kcl_io_backport.h>
#include <kcl/backport/kcl_uaccess_backport.h>
#include <kcl/kcl_seq_file.h>
#include <kcl/kcl_seqlock.h>
#include <kcl/kcl_mmu_notifier.h>
#include <kcl/kcl_preempt.h>
#include <kcl/kcl_suspend.h>
#include <kcl/backport/kcl_pci_backport.h>
#include <kcl/kcl_timekeeping.h>
#include <kcl/backport/kcl_mm_backport.h>
#include <kcl/kcl_pm.h>
#include <kcl/backport/kcl_fence_backport.h>
#include <kcl/backport/kcl_ttm_tt_backport.h>
#include <kcl/backport/kcl_device_cgroup_backport.h>
#include <kcl/kcl_mn.h>
#ifdef HAVE_DRM_DRIVER_GEM_PRIME_RES_OBJ
#include <linux/reservation.h>
#endif
#include <kcl/backport/kcl_drm_cache_backport.h>
#include <kcl/kcl_task_barrier.h>
#include <kcl/kcl_drm_print.h>
#include <kcl/backport/kcl_drm_backport.h>
#include <kcl/backport/kcl_drm_gem.h>
#include <kcl/backport/kcl_drm_vma_manager_backport.h>
#include <kcl/backport/kcl_drm_fb.h>
#include <kcl/kcl_drm_connector.h>
#include <kcl/backport/kcl_drm_dp_helper_backport.h>
#include <kcl/backport/kcl_drm_dp_mst_helper_backport.h>
#include <kcl/backport/kcl_drm_atomic_helper_backport.h>
#include <kcl/kcl_drm_hdcp.h>
#include <kcl/kcl_acpi_table.h>

#endif /* AMDGPU_BACKPORT_H */
