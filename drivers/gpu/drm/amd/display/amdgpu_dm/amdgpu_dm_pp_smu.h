/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#ifndef __AMDGPU_DM_PP_SMU_H__
#define __AMDGPU_DM_PP_SMU_H__

#include "dm_pp_interface.h"

#if IS_ENABLED(CONFIG_DRM_AMD_DC_KUNIT_TEST)
void get_default_clock_levels(enum dm_pp_clock_type clk_type, struct dm_pp_clock_levels *clks);
enum amd_pp_clock_type dc_to_pp_clock_type(enum dm_pp_clock_type dm_pp_clk_type);
#endif

#endif /* __AMDGPU_DM_PP_SMU_H__ */
