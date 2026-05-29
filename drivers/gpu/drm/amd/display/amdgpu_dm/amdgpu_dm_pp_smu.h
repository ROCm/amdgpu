/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#ifndef __AMDGPU_DM_PP_SMU_H__
#define __AMDGPU_DM_PP_SMU_H__

#include "dm_pp_interface.h"

struct amd_pp_display_configuration;
struct pp_smu_wm_range_sets;
struct dm_pp_wm_sets_with_clock_ranges_soc15;

#if IS_ENABLED(CONFIG_DRM_AMD_DC_KUNIT_TEST)
void build_pm_display_cfg(struct amd_pp_display_configuration *pm_display_cfg,
			  const struct dm_pp_display_configuration *pp_display_cfg);
void build_wm_clock_ranges_soc15(const struct pp_smu_wm_range_sets *ranges,
		struct dm_pp_wm_sets_with_clock_ranges_soc15 *wm_with_clock_ranges);
void get_default_clock_levels(enum dm_pp_clock_type clk_type, struct dm_pp_clock_levels *clks);
enum amd_pp_clock_type dc_to_pp_clock_type(enum dm_pp_clock_type dm_pp_clk_type);
void pp_to_dc_clock_levels(const struct amd_pp_clocks *pp_clks,
			   struct dm_pp_clock_levels *dc_clks,
			   enum dm_pp_clock_type dc_clk_type);
void pp_to_dc_clock_levels_with_latency(const struct pp_clock_levels_with_latency *pp_clks,
					struct dm_pp_clock_levels_with_latency *clk_level_info,
					enum dm_pp_clock_type dc_clk_type);
void pp_to_dc_clock_levels_with_voltage(const struct pp_clock_levels_with_voltage *pp_clks,
					struct dm_pp_clock_levels_with_voltage *clk_level_info,
					enum dm_pp_clock_type dc_clk_type);
void cap_clock_levels_to_validation(struct dm_pp_clock_levels *dc_clks,
				    enum dm_pp_clock_type clk_type,
				    const struct amd_pp_simple_clock_info *validation_clks);
bool pp_smu_nv_clock_id_to_pp(enum pp_smu_nv_clock_id clock_id,
			      enum amd_pp_clock_type *clock_type);
#endif

#endif /* __AMDGPU_DM_PP_SMU_H__ */
