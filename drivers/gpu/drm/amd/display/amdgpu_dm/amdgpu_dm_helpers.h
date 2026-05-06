/* SPDX-License-Identifier: MIT */
/*
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#ifndef __AMDGPU_DM_HELPERS_H__
#define __AMDGPU_DM_HELPERS_H__

#if IS_ENABLED(CONFIG_DRM_AMD_DC_KUNIT_TEST)
#include <drm/drm_edid.h>

/* Exported for KUnit testing */
u32 edid_extract_panel_id(struct edid *edid);
uint8_t get_max_frl_rate(uint8_t max_lanes, uint8_t max_rate_per_lane);
bool dm_is_freesync_pcon_whitelist(const uint32_t branch_dev_id);
extern const uint32_t dm_freesync_pcon_whitelist[];
uint32_t dm_freesync_pcon_whitelist_count(void);
#endif /* CONFIG_DRM_AMD_DC_KUNIT_TEST */

#endif /* __AMDGPU_DM_HELPERS_H__ */
