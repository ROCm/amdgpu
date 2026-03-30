// SPDX-License-Identifier: MIT
//
// Copyright 2026 Advanced Micro Devices, Inc.

#include "dml2_pmo_dcn42.h"
#include "lib_float_math.h"
#include "dml2_debug.h"
#include "dml2_pmo_dcn4_fams2.h"

/*
 * DCN42 PMO Policy Implementation
 * This implementation provides VBlank-only strategies for 1, 2, 3, and 4 display
 * configurations, ensuring p-state watermark support in the blank period only.
 */

static const double MIN_VACTIVE_MARGIN_PCT = 0.25; // We need more than non-zero margin because DET buffer granularity can alter vactive latency hiding

static const struct dml2_pmo_pstate_strategy dcn42_strategy_list_1_display[] = {
	// VBlank only
	{
		.per_stream_pstate_method = { dml2_pstate_method_vactive, dml2_pstate_method_na, dml2_pstate_method_na, dml2_pstate_method_na },
		.allow_state_increase = true,
	},
};

static const int dcn42_strategy_list_1_display_size = sizeof(dcn42_strategy_list_1_display) / sizeof(struct dml2_pmo_pstate_strategy);

static const struct dml2_pmo_pstate_strategy dcn42_strategy_list_2_display[] = {
	// VBlank only for both displays
	{
		.per_stream_pstate_method = { dml2_pstate_method_vactive, dml2_pstate_method_vactive, dml2_pstate_method_na, dml2_pstate_method_na },
		.allow_state_increase = true,
	},
};

static const int dcn42_strategy_list_2_display_size = sizeof(dcn42_strategy_list_2_display) / sizeof(struct dml2_pmo_pstate_strategy);

static const struct dml2_pmo_pstate_strategy dcn42_strategy_list_3_display[] = {
	// VBlank only for all three displays
	{
		.per_stream_pstate_method = { dml2_pstate_method_vactive, dml2_pstate_method_vactive, dml2_pstate_method_vactive, dml2_pstate_method_na },
		.allow_state_increase = true,
	},
};

static const int dcn42_strategy_list_3_display_size = sizeof(dcn42_strategy_list_3_display) / sizeof(struct dml2_pmo_pstate_strategy);

static const struct dml2_pmo_pstate_strategy dcn42_strategy_list_4_display[] = {
	// VBlank only for all four displays
	{
		.per_stream_pstate_method = { dml2_pstate_method_vactive, dml2_pstate_method_vactive, dml2_pstate_method_vactive, dml2_pstate_method_vactive },
		.allow_state_increase = true,
	},
};

static const int dcn42_strategy_list_4_display_size = sizeof(dcn42_strategy_list_4_display) / sizeof(struct dml2_pmo_pstate_strategy);

static bool is_bit_set_in_bitfield(unsigned int bit_field, unsigned int bit_offset)
{
	if (bit_field & (0x1 << bit_offset))
		return true;

	return false;
}

static void setup_planes_for_vactive_by_mask(struct display_configuation_with_meta *display_config,
	struct dml2_pmo_instance *pmo,
	int plane_mask)
{
	unsigned int plane_index;
	unsigned int stream_index;
	struct dml2_plane_parameters *plane;

	for (plane_index = 0; plane_index < display_config->display_config.num_planes; plane_index++) {
		if (is_bit_set_in_bitfield(plane_mask, plane_index)) {
			plane = &display_config->display_config.plane_descriptors[plane_index];
			stream_index = display_config->display_config.plane_descriptors[plane_index].stream_index;

			plane->overrides.reserved_vblank_time_ns = (long)math_max2(pmo->soc_bb->power_management_parameters.dram_clk_change_blackout_us * 1000.0,
					plane->overrides.reserved_vblank_time_ns);
			if (!pmo->options->disable_vactive_det_fill_bw_pad) {
				display_config->display_config.plane_descriptors[plane_index].overrides.max_vactive_det_fill_delay_us[dml2_pstate_type_uclk] =
					(unsigned int)math_floor(pmo->scratch.pmo_dcn4.stream_pstate_meta[stream_index].method_vactive.max_vactive_det_fill_delay_us);
			}

			display_config->stage3.pstate_switch_modes[plane_index] = dml2_pstate_method_vactive;
		}
	}
}

static void reset_display_configuration(struct display_configuation_with_meta *display_config)
{
	unsigned int plane_index;
	unsigned int stream_index;
	struct dml2_plane_parameters *plane;

	for (stream_index = 0; stream_index < display_config->display_config.num_streams; stream_index++) {
		display_config->stage3.stream_svp_meta[stream_index].valid = false;
	}

	for (plane_index = 0; plane_index < display_config->display_config.num_planes; plane_index++) {
		plane = &display_config->display_config.plane_descriptors[plane_index];

		// Unset SubVP
		plane->overrides.legacy_svp_config = dml2_svp_mode_override_auto;

		// Remove reserve time
		plane->overrides.reserved_vblank_time_ns = 0;

		// Reset strategy to auto
		plane->overrides.uclk_pstate_change_strategy = dml2_uclk_pstate_change_strategy_auto;

		display_config->stage3.pstate_switch_modes[plane_index] = dml2_pstate_method_na;
	}
}

static bool setup_display_config(struct display_configuation_with_meta *display_config, struct dml2_pmo_instance *pmo, int strategy_index)
{
	struct dml2_pmo_scratch *scratch = &pmo->scratch;

	bool fams2_required = false;
	bool success = true;
	unsigned int stream_index;

	reset_display_configuration(display_config);

	for (stream_index = 0; stream_index < display_config->display_config.num_streams; stream_index++) {

		if (pmo->scratch.pmo_dcn4.pstate_strategy_candidates[strategy_index].per_stream_pstate_method[stream_index] == dml2_pstate_method_na) {
			success = false;
			break;
		} else if (scratch->pmo_dcn4.pstate_strategy_candidates[strategy_index].per_stream_pstate_method[stream_index] == dml2_pstate_method_vactive) {
			setup_planes_for_vactive_by_mask(display_config, pmo, scratch->pmo_dcn4.stream_plane_mask[stream_index]);
		}
	}

	/* copy FAMS2 meta */
	if (success) {
		display_config->stage3.fams2_required = fams2_required;
		memcpy(&display_config->stage3.stream_pstate_meta,
			&scratch->pmo_dcn4.stream_pstate_meta,
			sizeof(struct dml2_pstate_meta) * DML2_MAX_PLANES);
	}

	return success;
}

bool pmo_dcn42_fams2_optimize_for_pstate_support(struct dml2_pmo_optimize_for_pstate_support_in_out *in_out)
{
	bool success = false;
	struct dml2_pmo_scratch *s = &in_out->instance->scratch;

	memcpy(in_out->optimized_display_config, in_out->base_display_config, sizeof(struct display_configuation_with_meta));

	if (in_out->last_candidate_failed) {
		if (s->pmo_dcn4.pstate_strategy_candidates[s->pmo_dcn4.cur_pstate_candidate].allow_state_increase &&
			s->pmo_dcn4.cur_latency_index < s->pmo_dcn4.max_latency_index - 1) {
			s->pmo_dcn4.cur_latency_index++;

			success = true;
		}
	}

	if (!success) {
		s->pmo_dcn4.cur_latency_index = s->pmo_dcn4.min_latency_index;
		s->pmo_dcn4.cur_pstate_candidate++;

		if (s->pmo_dcn4.cur_pstate_candidate < s->pmo_dcn4.num_pstate_candidates) {
			success = true;
		}
	}

	if (success) {
		in_out->optimized_display_config->stage3.min_clk_index_for_latency = s->pmo_dcn4.cur_latency_index;
		setup_display_config(in_out->optimized_display_config, in_out->instance, in_out->instance->scratch.pmo_dcn4.cur_pstate_candidate);
	}

	return success;
}

bool pmo_dcn42_test_for_pstate_support(struct dml2_pmo_test_for_pstate_support_in_out *in_out)
{
	const struct dml2_pmo_scratch *s = &in_out->instance->scratch;
	bool p_state_supported = true;
	unsigned int stream_index;

	if (s->pmo_dcn4.cur_pstate_candidate < 0)
		return false;

	for (stream_index = 0; stream_index < in_out->base_display_config->display_config.num_streams; stream_index++) {
		if (s->pmo_dcn4.pstate_strategy_candidates[s->pmo_dcn4.cur_pstate_candidate].per_stream_pstate_method[stream_index] == dml2_pstate_method_vactive) {
			if (dcn4_get_minimum_reserved_time_us_for_planes(in_out->base_display_config, s->pmo_dcn4.stream_plane_mask[stream_index]) < (int)in_out->instance->soc_bb->power_management_parameters.dram_clk_change_blackout_us ||
			    dcn4_get_vactive_pstate_margin(in_out->base_display_config, s->pmo_dcn4.stream_plane_mask[stream_index]) < (int)(MIN_VACTIVE_MARGIN_PCT * in_out->instance->soc_bb->power_management_parameters.dram_clk_change_blackout_us)) {
				p_state_supported = false;
				break;
			}
		} else {
			p_state_supported = false;
			break;
		}
	}

	return p_state_supported;
}

bool pmo_dcn42_initialize(struct dml2_pmo_initialize_in_out *in_out)
{
	int i = 0;
	struct dml2_pmo_instance *pmo = in_out->instance;

	unsigned int base_list_size = 0;
	const struct dml2_pmo_pstate_strategy *base_list = NULL;
	unsigned int *expanded_list_size = NULL;
	struct dml2_pmo_pstate_strategy *expanded_list = NULL;

	DML_LOG_COMP_IF_ENTER();

	pmo->soc_bb = in_out->soc_bb;
	pmo->ip_caps = in_out->ip_caps;
	pmo->mpc_combine_limit = 2;
	pmo->odm_combine_limit = 4;
	pmo->mcg_clock_table_size = in_out->mcg_clock_table_size;

	/*
	 * DCN42 does not support FAMS features like SubVP and DRR.
	 * These parameters are initialized to safe values but won't be used
	 * since our strategies only use VBlank.
	 */
	pmo->fams_params.v2.subvp.refresh_rate_limit_max = 0;
	pmo->fams_params.v2.subvp.refresh_rate_limit_min = 0;
	pmo->fams_params.v2.drr.refresh_rate_limit_max = 0;
	pmo->fams_params.v2.drr.refresh_rate_limit_min = 0;

	pmo->options = in_out->options;

	/* Generate permutations of p-state configs from base strategy list */
	for (i = 0; i < PMO_DCN4_MAX_DISPLAYS; i++) {
		switch (i+1) {
		case 1:
			if (pmo->options->override_strategy_lists[i] && pmo->options->num_override_strategies_per_list[i]) {
				base_list = pmo->options->override_strategy_lists[i];
				base_list_size = pmo->options->num_override_strategies_per_list[i];
			} else {
				base_list = dcn42_strategy_list_1_display;
				base_list_size = dcn42_strategy_list_1_display_size;
			}

			expanded_list_size = &pmo->init_data.pmo_dcn4.num_expanded_strategies_per_list[i];
			expanded_list = pmo->init_data.pmo_dcn4.expanded_strategy_list_1_display;

			break;
		case 2:
			if (pmo->options->override_strategy_lists[i] && pmo->options->num_override_strategies_per_list[i]) {
				base_list = pmo->options->override_strategy_lists[i];
				base_list_size = pmo->options->num_override_strategies_per_list[i];
			} else {
				base_list = dcn42_strategy_list_2_display;
				base_list_size = dcn42_strategy_list_2_display_size;
			}

			expanded_list_size = &pmo->init_data.pmo_dcn4.num_expanded_strategies_per_list[i];
			expanded_list = pmo->init_data.pmo_dcn4.expanded_strategy_list_2_display;

			break;
		case 3:
			if (pmo->options->override_strategy_lists[i] && pmo->options->num_override_strategies_per_list[i]) {
				base_list = pmo->options->override_strategy_lists[i];
				base_list_size = pmo->options->num_override_strategies_per_list[i];
			} else {
				base_list = dcn42_strategy_list_3_display;
				base_list_size = dcn42_strategy_list_3_display_size;
			}

			expanded_list_size = &pmo->init_data.pmo_dcn4.num_expanded_strategies_per_list[i];
			expanded_list = pmo->init_data.pmo_dcn4.expanded_strategy_list_3_display;

			break;
		case 4:
			if (pmo->options->override_strategy_lists[i] && pmo->options->num_override_strategies_per_list[i]) {
				base_list = pmo->options->override_strategy_lists[i];
				base_list_size = pmo->options->num_override_strategies_per_list[i];
			} else {
				base_list = dcn42_strategy_list_4_display;
				base_list_size = dcn42_strategy_list_4_display_size;
			}

			expanded_list_size = &pmo->init_data.pmo_dcn4.num_expanded_strategies_per_list[i];
			expanded_list = pmo->init_data.pmo_dcn4.expanded_strategy_list_4_display;

			break;
		}

		DML_ASSERT(base_list_size <= PMO_DCN4_MAX_BASE_STRATEGIES);

		/*
		 * Populate list using DCN4 FAMS2 expansion function.
		 * Since our strategies only contain VBlank methods, the expansion
		 * will not introduce any FAMS-specific logic.
		 */
		pmo_dcn4_fams2_expand_base_pstate_strategies(
				base_list,
				base_list_size,
				i + 1,
				expanded_list,
				expanded_list_size);
	}

	DML_LOG_DEBUG("%s exit with true\n", __func__);
	DML_LOG_COMP_IF_EXIT();

	return true;
}
