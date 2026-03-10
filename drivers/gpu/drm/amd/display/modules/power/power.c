/*
 * Copyright 2016 Advanced Micro Devices, Inc.
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
 * Authors: AMD
 *
 */

#include "dm_services.h"
#include "dc.h"
#include "mod_power.h"
#include "core_types.h"
#include "dmcu.h"
#include "abm.h"
#include "power_helpers.h"
#include "dce/dmub_psr.h"
#include "dal_asic_id.h"
#include "link_service.h"
#include <linux/math.h>

#define DC_TRACE_LEVEL_MESSAGE(...) /* do nothing */
#define DC_TRACE_LEVEL_MESSAGEP(...) /* do nothing */

#define MOD_POWER_MAX_CONCURRENT_STREAMS 32
#define SMOOTH_BRIGHTNESS_ADJUSTMENT_TIME_IN_MS 500
#define LOW_REFRESH_RATE_DURATION_US_UPPER_BOUND 25000


struct backlight_state {
	/* HW uses u16.16 format for backlight PWM */
	unsigned int backlight_pwm;
	/* DM may call power module to set backlight
	 * targeting percent brightness
	 */
	unsigned int backlight_millipercent;
	/* DM may call power module to set backlight based on an explicit
	 * nits value.
	 */
	unsigned int backlight_millinit;
	unsigned int frame_ramp;
	bool smooth_brightness_enabled;
	bool isHDR;
};
struct power_entity {
	struct dc_stream_state *stream;
	struct psr_caps *caps;
	struct mod_power_psr_context *psr_context;

	/*PSR cached properties*/
	bool psr_enabled;
	unsigned int psr_events;
	unsigned int psr_power_opt;
	unsigned int replay_events;
};

struct pwr_backlight_properties {
	bool use_nits_based_brightness;
	bool disable_fractional_pwm;

	unsigned int min_abm_backlight;
	unsigned int num_backlight_levels;

	bool backlight_ramping_override;
	unsigned int backlight_ramping_reduction;
	unsigned int backlight_ramping_start;

	/* Backlight cached properties */
	unsigned int ac_backlight_percent;
	unsigned int dc_backlight_percent;

	/* backlight LUT stored in HW u16.16 format*/
	unsigned int *backlight_lut;
	unsigned int min_backlight_pwm;
	unsigned int max_backlight_pwm;
	unsigned int backlight_range;

	/* Describes the panel's min and max luminance in millinits measured
	 * on full white screen, in min and max backlight settings.
	 */
	unsigned int min_brightness_millinits;
	unsigned int max_brightness_millinits;
	unsigned int nits_range;

	bool backlight_caps_valid;
	bool use_custom_backlight_caps;
	unsigned int custom_backlight_caps_config_no;
	bool use_linear_backlight_curve;
};

struct dmcu_varibright_cached_properties {
	unsigned int varibright_config_setting;
	unsigned int varibright_level;
	unsigned int varibright_hw_level;
	unsigned int def_varibright_level;
	bool varibright_user_enable;
	bool varibright_active;
};

struct core_power {
	struct mod_power mod_public;
	struct dc *dc;
	struct power_entity *map;
	struct dmcu_varibright_cached_properties varibright_prop;
	struct pwr_backlight_properties bl_prop[MAX_NUM_EDP];
	struct backlight_state bl_state[MAX_NUM_EDP];
	unsigned int edp_num;

	bool psr_smu_optimizations_support;
	bool multi_disp_optimizations_support;

	unsigned int num_entities;
};

union dmcu_abm_set_bl_params {
	struct {
		unsigned int gradual_change : 1; /* [0:0] */
		unsigned int reserved : 15; /* [15:1] */
		unsigned int frame_ramp : 16; /* [31:16] */
	} bits;
	unsigned int u32All;
};

/* If system or panel does not report some sort of brightness percent to nits
 * mapping, we will use following default values so backlight control using
 * nits based interfaces will still work, but might not describe panel
 * correctly. In this case percentage based backlight control should ideally
 * be used.
 * Min = 5 nits
 * Max = 300 nits
 */

static const unsigned int pwr_default_min_brightness_millinits = 1000;
static const unsigned int pwr_default_sdr_brightness_millinits = 270000;

static const unsigned int default_ac_backlight_percent   = 100;
static const unsigned int default_dc_backlight_percent   = 70;

#define MOD_POWER_TO_CORE(mod_power)\
		container_of(mod_power, struct core_power, mod_public)

static unsigned int calc_psr_num_static_frames(unsigned int vsync_rate_hz)
{
	/* Initialize fail-safe to 2 static frames. */
	unsigned int num_frames_static = 2;

	/* Calculate number of frames such that at least 30 ms has passed.
	 * Round up to ensure the static period is not shorter than 30 ms.
	 */
	if (vsync_rate_hz != 0)
		num_frames_static = DIV_ROUND_UP(30000 * vsync_rate_hz, 1000000);

	return num_frames_static;
}

/* Given a specific dc_stream* this function finds its equivalent
 * on the core_freesync->map and returns the corresponding index
 */
static unsigned int map_index_from_stream(struct core_power *core_power,
		const struct dc_stream_state *stream)
{
	unsigned int index = 0;

	for (index = 0; index < core_power->num_entities; index++) {
		if (core_power->map[index].stream == stream)
			return index;
	}
	/* Could not find stream requested, this is not trivial, fix when hit*/
	DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
						WPP_BIT_FLAG_Firmware_PsrState,
						"map index from stream: ERROR: core_power=%p stream=%p",
						core_power,
						stream);
	ASSERT(false);
	/* We come here only when we can't map stream index.
	 * In good cases, this would happen when we attempt to change
	 * brightness before stream creation, in which case we create a
	 * dummy stream with index 0.
	 * With external monitor connected, the index passed from this return
	 * is 1. Passing anything greater than 0 from here would always point
	 * to bad memory.
	 */
	return 0;
}

bool mod_power_hw_init(struct mod_power *mod_power)
{
	/* Call backlight initialization */
	return mod_power_hw_init_backlight(mod_power);

	/* Future: Add other HW init here */
}

struct mod_power *mod_power_create(struct dc *dc,
		struct mod_power_init_params *init_params,
		unsigned int edp_num)
{
	struct core_power *core_power = NULL;
	int i = 0;
	unsigned int abm_max_config = 0;
	unsigned int inst = 0;
	bool is_brightness_range_valid = false;

	if (dc == NULL)
		goto fail_dc_null;

	core_power = kzalloc(sizeof(struct core_power), GFP_KERNEL);

	if (core_power == NULL)
		goto fail_alloc_context;

	core_power->edp_num = edp_num;
	core_power->map = kzalloc(sizeof(struct power_entity) * MOD_POWER_MAX_CONCURRENT_STREAMS,
				  GFP_KERNEL);

	if (core_power->map == NULL)
		goto fail_alloc_map;

	for (i = 0; i < MOD_POWER_MAX_CONCURRENT_STREAMS; i++) {
		core_power->map[i].stream = NULL;
	}

	for (i = 0; i < MOD_POWER_MAX_CONCURRENT_STREAMS; i++) {
		core_power->map[i].psr_context =
				kzalloc(sizeof(struct mod_power_psr_context),
					GFP_KERNEL);
		if (core_power->map[i].psr_context == NULL)
			goto fail_construct;
	}

	core_power->psr_smu_optimizations_support = init_params->allow_psr_smu_optimizations;
	core_power->multi_disp_optimizations_support = init_params->allow_psr_multi_disp_optimizations;

	for (inst = 0; inst < edp_num; inst++) {
		core_power->bl_prop[inst].min_abm_backlight =
				init_params[inst].min_abm_backlight;
		core_power->bl_prop[inst].disable_fractional_pwm =
				init_params[inst].disable_fractional_pwm;
		core_power->bl_prop[inst].use_linear_backlight_curve =
				init_params[inst].use_linear_backlight_curve;
		core_power->bl_prop[inst].use_nits_based_brightness =
				init_params[inst].use_nits_based_brightness;
		core_power->bl_prop[inst].backlight_ramping_override =
				init_params[inst].backlight_ramping_override;
		core_power->bl_prop[inst].backlight_ramping_reduction =
				init_params[inst].backlight_ramping_reduction;
		core_power->bl_prop[inst].backlight_ramping_start =
				init_params[inst].backlight_ramping_start;
		core_power->bl_prop[inst].use_custom_backlight_caps =
				init_params[inst].use_custom_backlight_caps;
		core_power->bl_prop[inst].custom_backlight_caps_config_no =
				init_params[inst].custom_backlight_caps_config_no;

		// Do not allow less than 101 backlight levels
		if (init_params[inst].num_backlight_levels < 101)
			core_power->bl_prop[inst].num_backlight_levels = 101;
		else
			core_power->bl_prop[inst].num_backlight_levels =
				init_params[inst].num_backlight_levels;

		core_power->bl_prop[inst].backlight_lut = (unsigned int *)
				(kzalloc(sizeof(unsigned int) *
				core_power->bl_prop[inst].num_backlight_levels, GFP_KERNEL));
		if (core_power->bl_prop[inst].backlight_lut == NULL)
			goto fail_alloc_backlight_array;
	}

	core_power->varibright_prop.varibright_active = false;

	core_power->varibright_prop.varibright_user_enable =
			init_params->def_varibright_enable;

	// Table of ABM levels here is 1-4, but level 0 also exists as 'off'
	if (init_params->varibright_level <= abm_defines_max_level) {
		core_power->varibright_prop.varibright_level =
			init_params->varibright_level;

	} else {
		core_power->varibright_prop.varibright_level = 3;
	}
	if (init_params->def_varibright_level <= abm_defines_max_level) {
		core_power->varibright_prop.def_varibright_level =
			init_params->def_varibright_level;
	} else {
		core_power->varibright_prop.def_varibright_level = 3;
	}

	// ABM used to contain 4 different configs. There is only 3 since ABM 2.3.
	if ((dc->res_pool->dmcu != NULL) && (dc->res_pool->dmcu->dmcu_version.abm_version < 0x23))
		abm_max_config = 4;
	else
		abm_max_config = 3;

	if (init_params->abm_config_setting < abm_max_config)
		core_power->varibright_prop.varibright_config_setting =
			init_params->abm_config_setting;
	else
		core_power->varibright_prop.varibright_config_setting = 0;

	for (inst = 0; inst < edp_num; inst++) {
		core_power->bl_prop[inst].backlight_lut[0] = init_params[inst].min_backlight_pwm;
		core_power->bl_prop[inst].backlight_lut[
			core_power->bl_prop[inst].num_backlight_levels-1] =
				init_params[inst].max_backlight_pwm;
		core_power->bl_prop[inst].min_backlight_pwm = init_params[inst].min_backlight_pwm;
		core_power->bl_prop[inst].max_backlight_pwm = init_params[inst].max_backlight_pwm;
		core_power->bl_prop[inst].ac_backlight_percent =
				default_ac_backlight_percent;
		core_power->bl_prop[inst].dc_backlight_percent =
				default_dc_backlight_percent;
		core_power->bl_prop[inst].backlight_caps_valid = false;

		if (core_power->bl_prop[inst].use_nits_based_brightness) {
			core_power->bl_prop[inst].min_brightness_millinits =
					init_params[inst].panel_min_millinits;
			core_power->bl_prop[inst].max_brightness_millinits =
					init_params[inst].panel_max_millinits;
		} else {

			core_power->bl_prop[inst].min_brightness_millinits =
					pwr_default_min_brightness_millinits;
			core_power->bl_prop[inst].max_brightness_millinits =
					pwr_default_sdr_brightness_millinits;
		}

		core_power->bl_prop[inst].backlight_range =
				core_power->bl_prop[inst].max_backlight_pwm-
				core_power->bl_prop[inst].min_backlight_pwm;

		core_power->bl_prop[inst].nits_range =
				core_power->bl_prop[inst].max_brightness_millinits -
				core_power->bl_prop[inst].min_brightness_millinits;

		core_power->bl_state[inst].smooth_brightness_enabled = true;
	}

	/* Check if at least 1 instance in core_power is populated before failing */
	for (inst = 0; inst < edp_num; inst++) {
		if (core_power->bl_prop[inst].nits_range != 0 && core_power->bl_prop[inst].backlight_range != 0) {
			is_brightness_range_valid = true;
			break;
		}

	}
	if (!is_brightness_range_valid)
		goto fail_bad_brightness_range;

	core_power->num_entities = 0;

	core_power->dc = dc;
	for (inst = 0; inst < edp_num; inst++) {
		initialize_backlight_caps(core_power, inst);
		core_power->bl_state[inst].backlight_millipercent =
			core_power->bl_prop[inst].dc_backlight_percent * 1000;
		core_power->bl_state[inst].backlight_pwm = backlight_millipercent_to_pwm(core_power,
		core_power->bl_state[inst].backlight_millipercent, inst);
		core_power->bl_state[inst].backlight_millinit = backlight_millipercent_to_millinit(core_power,
		core_power->bl_state[inst].backlight_millipercent, inst);
	}

	return &core_power->mod_public;

fail_bad_brightness_range:
fail_alloc_backlight_array:
	for (inst = 0; inst < edp_num; inst++)
		if (core_power->bl_prop[inst].backlight_lut)
			kfree(core_power->bl_prop[inst].backlight_lut);
fail_construct:
	for (i = 0; i < MOD_POWER_MAX_CONCURRENT_STREAMS; i++) {
		if (core_power->map[i].psr_context)
			kfree(core_power->map[i].psr_context);
	}
	kfree(core_power->map);

fail_alloc_map:
	kfree(core_power);

fail_alloc_context:
fail_dc_null:
	return NULL;
}

void mod_power_destroy(struct mod_power *mod_power)
{
	if (mod_power != NULL) {
		unsigned int i;
		struct core_power *core_power =
				MOD_POWER_TO_CORE(mod_power);

		for (i = 0; i < MOD_POWER_MAX_CONCURRENT_STREAMS; i++)
			if (core_power->map[i].psr_context)
				kfree(core_power->map[i].psr_context);

		for (i = 0; i < core_power->num_entities; i++)
			if (core_power->map[i].stream)
				dc_stream_release(core_power->map[i].stream);

		kfree(core_power->map);

		for (i = 0; i < MAX_NUM_EDP; i++)
			if (core_power->bl_prop[i].backlight_lut)
				kfree(core_power->bl_prop[i].backlight_lut);

		kfree(core_power);
	}
}

bool mod_power_add_stream(struct mod_power *mod_power,
		struct dc_stream_state *stream, struct psr_caps *caps)
{
	struct core_power *core_power = NULL;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities < MOD_POWER_MAX_CONCURRENT_STREAMS) {
		dc_stream_retain(stream);

		core_power->map[core_power->num_entities].stream = stream;
		core_power->map[core_power->num_entities].caps = caps;

		// initialize cached PSR params to something "safe" (something that is
		// consistent with disabled PSR state)
		core_power->map[core_power->num_entities].psr_enabled = 0;
		core_power->map[core_power->num_entities].psr_events = psr_event_vsync;
		core_power->map[core_power->num_entities].psr_power_opt = 0;
		core_power->num_entities++;
		return true;
	}

	DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
						WPP_BIT_FLAG_Firmware_PsrState,
						"mod_power: add_stream: ERROR: stream=%p num_entities=%u >= MOD_POWER_MAX_CONCURRENT_STREAMS",
						stream,
						core_power->num_entities);

	return false;
}

bool mod_power_remove_stream(struct mod_power *mod_power,
		const struct dc_stream_state *stream)
{
	unsigned int i = 0;
	struct core_power *core_power = NULL;
	unsigned int index = 0;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);
	if (core_power->num_entities == 0) {
		/* trying to remove a stream a second time or have not added yet */
		BREAK_TO_DEBUGGER();
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
							WPP_BIT_FLAG_Firmware_PsrState,
							"mod_power: remove_stream: ERROR: num_entities=0 stream=%p",
							stream);
		return false;
	}

	index = map_index_from_stream(core_power, stream);

	if (index >= core_power->num_entities) {
		/* trying to remove a stream a second time or have not added yet */
		BREAK_TO_DEBUGGER();
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
							WPP_BIT_FLAG_Firmware_PsrState,
							"mod_power: remove_stream: ERROR: index=%u >= num_entities=%u stream=%p",
							index,
							core_power->num_entities,
							stream);
		return false;
	}

	dc_stream_release(core_power->map[index].stream);
	core_power->map[index].stream = NULL;
	/* To remove this entity, shift everything after down */
	for (i = index; i < core_power->num_entities - 1; i++) {
		core_power->map[i].stream = core_power->map[i + 1].stream;
		core_power->map[i].caps = core_power->map[i + 1].caps;

		// copy over cached parameters in case they map to PSR capable display
		core_power->map[i].psr_enabled = core_power->map[i + 1].psr_enabled;
		core_power->map[i].psr_events = core_power->map[i + 1].psr_events;
		core_power->map[i].psr_power_opt = core_power->map[i + 1].psr_power_opt;

		memcpy(core_power->map[i].psr_context, core_power->map[i + 1].psr_context, sizeof(struct mod_power_psr_context));
		memset(core_power->map[i + 1].psr_context, 0, sizeof(struct mod_power_psr_context));
	}
	core_power->num_entities--;

	return true;
}

/*
 * Replace_stream should be used when there is a mode set for existing
 * display target with a valid stream. In this case might need to retain
 * cached PSR state (events, power opt, en/dis) if we are dealing with PSR
 * capable display. If mod_power_remove and mod_power_add are used instead,
 * then stream may be assigned to a different slot and may end up with
 * wrong cached PSR state. It is hard to tell which PSR events should
 * persist through mode set or what psr_events should be initialized to, so
 * it might be better just to retain them all.
 */
bool mod_power_replace_stream(struct mod_power *mod_power,
		const struct dc_stream_state *current_stream,
		struct dc_stream_state *new_stream,
		struct psr_caps *new_caps)
{
	struct core_power *core_power = NULL;
	unsigned int index = 0;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);
	if (core_power->num_entities == 0) {
		/* no streams exist in the table yet */
		BREAK_TO_DEBUGGER();
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
							WPP_BIT_FLAG_Firmware_PsrState,
							"mod_power: replace_stream: ERROR: num_entities=0 stream=%p",
							current_stream);
		return false;
	}

	index = map_index_from_stream(core_power, current_stream);

	if (index >= core_power->num_entities) {
		/* trying to replace a non-existent stream */
		BREAK_TO_DEBUGGER();
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
							WPP_BIT_FLAG_Firmware_PsrState,
							"mod_power: replace_stream: ERROR: index=%u >= num_entities=%u stream=%p",
							index,
							core_power->num_entities,
							current_stream);
		return false;
	}

	dc_stream_release(core_power->map[index].stream);
	dc_stream_retain(new_stream);
	core_power->map[index].stream = new_stream;
	core_power->map[index].caps = new_caps;
	memset(core_power->map[index].psr_context, 0, sizeof(struct mod_power_psr_context));

	return true;
}

bool mod_power_notify_mode_change(struct mod_power *mod_power,
		const struct dc_stream_state *stream,
		bool is_hdr)
{
	unsigned int stream_index = 0;
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	struct psr_config psr_config = {0};
	struct psr_context psr_context = {0};
	struct dc *dc = NULL;
	unsigned int panel_inst = 0;
	int active_psr_events = 0;
	int active_replay_events = 0;

	if ((mod_power == NULL) || (stream == NULL))
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	stream_index = map_index_from_stream(core_power, stream);

	if (stream_index >= core_power->num_entities)
		return false;

	dc = core_power->dc;
	link = dc_stream_get_link(stream);
	active_psr_events = core_power->map[stream_index].psr_events;
	active_replay_events = core_power->map[stream_index].replay_events;
	if (link != NULL && dc_get_edp_link_panel_inst(dc, link, &panel_inst)) {
		ASSERT(link->ddc->ddc_pin->hw_info.ddc_channel <= 0xFF);
		uint8_t aux_inst = (uint8_t)link->ddc->ddc_pin->hw_info.ddc_channel;

		mod_power_update_backlight_on_mode_change(core_power, link, panel_inst, aux_inst, is_hdr);

		mod_power_calc_psr_configs(&psr_config, link, stream);

		psr_config.psr_exit_link_training_required = core_power->map[stream_index].caps->psr_exit_link_training_required;

		if (dc->ctx->asic_id.chip_family >= AMDGPU_FAMILY_GC_11_0_1)
			psr_config.allow_smu_optimizations =
					core_power->psr_smu_optimizations_support && dc_is_embedded_signal(stream->signal);
		else
			psr_config.allow_smu_optimizations =
					core_power->psr_smu_optimizations_support && mod_power_only_edp(dc->current_state, stream);

		psr_config.allow_multi_disp_optimizations = core_power->multi_disp_optimizations_support;

		psr_config.rate_control_caps = core_power->map[stream_index].caps->rate_control_caps;

		if (active_psr_events & psr_event_os_request_force_ffu) {
			psr_config.os_request_force_ffu = true;
		}
		/*
		* DSC support:
		* DSC slice height value must be 'mod' by su_y_granularity.
		* According to Panel Vendor, there might be varied conditions to fulfill.
		* Right now, DSC slice height value must be multiple of su_y_granularity.
		*
		* The value of DSC slice height is determined in DSC Driver but it does not
		* propagated out here, so we need to calculate it as below 'slice_height'.
		*/
		psr_su_set_dsc_slice_height(dc, link,
					(struct dc_stream_state *) stream,
					&psr_config);

		dc_link_setup_psr(link, stream, &psr_config, &psr_context);

		link->replay_settings.replay_smu_opt_enable =
			(link->replay_settings.config.replay_smu_opt_supported &&
			mod_power_only_edp(dc->current_state, stream));

		if (active_replay_events & replay_event_os_request_force_ffu) {
			link->replay_settings.config.os_request_force_ffu = true;
		}

		if (dc_is_embedded_signal(stream->signal))
			dc->link_srv->dp_setup_replay(link, stream);
	}

	return true;
}

static void mod_power_psr_set_power_opt(struct mod_power *mod_power,
	struct dc_stream_state *stream,
	unsigned int active_psr_events,
	bool psr_enable_request)
{
	(void)psr_enable_request;
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	unsigned int stream_index = 0;
	unsigned int power_opt = 0;

	if (!stream)
		return;

	core_power = MOD_POWER_TO_CORE(mod_power);
	stream_index = map_index_from_stream(core_power, stream);
	if (!core_power->map[stream_index].caps->psr_version)
		return;

	link = dc_stream_get_link(stream);

	if (active_psr_events == 0) {
		/* Static Screen */
		power_opt |= (psr_power_opt_smu_opt_static_screen | psr_power_opt_z10_static_screen |
					psr_power_opt_ds_disable_allow);
	}

	/* psr_power_opt_flag is a configuration parameter into the module that determines
	 * which optimizations to enable during psr
	 */
	power_opt &= core_power->map[stream_index].caps->psr_power_opt_flag;
	if (core_power->map[stream_index].psr_power_opt != power_opt) {
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_VERBOSE,
				WPP_BIT_FLAG_Firmware_PsrState,
				"mod_power set_power_opt: psr_power_opt=0x%04x, power_opt=0x%04x"
				"active_psr_events=0x%04x, psr_power_opt_flag=0x%04x",
				core_power->map[stream_index].psr_power_opt,
				power_opt,
				active_psr_events,
				core_power->map[stream_index].caps->psr_power_opt_flag);
		dc_link_set_psr_allow_active(link, NULL, false, false, &power_opt);
		core_power->map[stream_index].psr_power_opt = power_opt;
	}
}

static bool set_psr_enable(struct mod_power *mod_power,
		struct dc_stream_state *stream,
		bool psr_enable,
		bool wait,
		bool force_static)
{
	struct core_power *core_power = NULL;
	enum dc_psr_state state = PSR_STATE0;
	unsigned int retry_count;
	const unsigned int max_retry = 1000;
	struct dc_link *link = NULL;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0) {
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
							WPP_BIT_FLAG_Firmware_PsrState,
							"set psr enable: ERROR: stream=%p num_entities=%u",
							stream,
							core_power->num_entities);
		return false;
	}

	if (psr_enable)	{
		unsigned int vsync_rate_hz;
		struct dc_static_screen_params params = {0};

		vsync_rate_hz = (unsigned int)div_u64(div_u64((
				stream->timing.pix_clk_100hz * 100),
				stream->timing.v_total),
				stream->timing.h_total);

		params.triggers.cursor_update = true;
		params.triggers.overlay_update = true;
		params.triggers.surface_update = true;
		params.num_frames = calc_psr_num_static_frames(vsync_rate_hz);

		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_INFORMATION,
							WPP_BIT_FLAG_Firmware_PsrState,
							"set psr enable: CALCS: pix_clk_100hz=%u v_total=%u h_total=%u vsync_rate_hz=%u num_frames=%u",
							stream->timing.pix_clk_100hz,
							stream->timing.v_total,
							stream->timing.h_total,
							vsync_rate_hz,
							params.num_frames);

		dc_stream_set_static_screen_params(core_power->dc,
						   &stream, 1,
						   &params);
	}

	link = dc_stream_get_link(stream);

	if (!dc_link_set_psr_allow_active(link, &psr_enable, false, force_static, NULL)) {
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
							WPP_BIT_FLAG_Firmware_PsrState,
							"set psr enable: ERROR: stream=%p link=%p psr_enable=%d",
							stream,
							link,
							psr_enable);
		return false;
	}

	if (wait == true) {

		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_INFORMATION,
							WPP_BIT_FLAG_Firmware_PsrState,
							"set psr enable: BEGIN WAIT: psr_enable=%d",
							(int)psr_enable);

		for (retry_count = 0; retry_count <= max_retry; retry_count++) {
			dc_link_get_psr_state(link, &state);
			if (psr_enable) {
				if (state != PSR_STATE0 &&
						(!force_static || state == PSR_STATE3))
					break;
			} else {
				if (state == PSR_STATE0)
					break;
			}
			udelay(500);
		}

		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_INFORMATION,
							WPP_BIT_FLAG_Firmware_PsrState,
							"set psr enable: END WAIT: psr_enable=%d",
							(int)psr_enable);

		/* assert if max retry hit */
		if (retry_count >= max_retry) {
			ASSERT(0);
			DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
								WPP_BIT_FLAG_Firmware_PsrState,
								"set psr enable: ERROR: retry_count=%u: Unexpectedly long wait for PSR state change.",
								retry_count);
		}
	} else {
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_INFORMATION,
							WPP_BIT_FLAG_Firmware_PsrState,
							"set psr enable: PSR state change initiated (wait=false): psr_enable=%d",
							(int)psr_enable);
	}

	return true;
}

bool mod_power_get_psr_event(struct mod_power *mod_power,
			struct dc_stream_state *stream,
			unsigned int *active_psr_events)
{
	struct core_power *core_power = NULL;
	unsigned int stream_index = 0;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	stream_index = map_index_from_stream(core_power, stream);

	if (!core_power->map[stream_index].caps->psr_version)
		return false;

	*active_psr_events = core_power->map[stream_index].psr_events;

	return true;
}

bool mod_power_set_psr_event(struct mod_power *mod_power,
		struct dc_stream_state *stream, bool set_event,
		enum psr_event event, bool wait)
{
	struct core_power *core_power = NULL;
	unsigned int stream_index = 0;
	unsigned int active_psr_events = 0;
	bool psr_enable_request = false;
	bool force_static = false;

	if (mod_power == NULL || stream == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);
	stream_index = map_index_from_stream(core_power, stream);

	if (core_power->num_entities == 0) {
		DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_ERROR,
							WPP_BIT_FLAG_Firmware_PsrState,
							"mod_power set_psr_event: ERROR: stream=%p event=%d num_entities=%u",
							stream,
							(int)event,
							core_power->num_entities);
		return false;
	}

	if (!core_power->map[stream_index].caps->psr_version)
		return false;

	if (set_event)
		core_power->map[stream_index].psr_events |= event;
	else
		core_power->map[stream_index].psr_events &= ~event;

	active_psr_events = core_power->map[stream_index].psr_events;

	// ignore other events when we're in forced psr enabled state
	if (active_psr_events & psr_event_dynamic_display_switch &&
			event != psr_event_dynamic_display_switch)
		return false;

	// ignore other events when we're in forced psr enabled state
	if (active_psr_events & psr_event_os_override_hold &&
			event != psr_event_os_override_hold)
		return false;

	// ignore other events when we're in forced psr enabled state
	// dds events need to be processed while in dynamic_link_rate_control
	if (active_psr_events & psr_event_dynamic_link_rate_control &&
			event != psr_event_dynamic_link_rate_control &&
			event != psr_event_dds_defer_stream_enable &&
			event != psr_event_dynamic_display_switch)
		return false;

	if (active_psr_events & (psr_event_test_harness_disable_psr | psr_event_os_request_disable))
		psr_enable_request = false;
	else if (active_psr_events & psr_event_pause)
		psr_enable_request = false;
	else if (active_psr_events & psr_event_test_harness_enable_psr)
		psr_enable_request = true;
	else if (active_psr_events & psr_event_dynamic_display_switch) {
		psr_enable_request = true;
		force_static = true;
	} else if (active_psr_events & psr_event_dynamic_link_rate_control) {
		psr_enable_request = true;
		force_static = true;
	} else if (active_psr_events & psr_event_edp_panel_off_disable_psr)
		psr_enable_request = false;
	else if (active_psr_events & (psr_event_hw_programming |
			psr_event_defer_enable |
			psr_event_dds_defer_stream_enable |
			psr_event_vrr_transition |
			psr_event_immediate_flip))
		psr_enable_request = false;
	else if (active_psr_events & psr_event_big_screen_video)
		psr_enable_request = true;
	else if (active_psr_events & psr_event_full_screen)
		psr_enable_request = false;
	else if (active_psr_events & psr_event_mpo_video_selective_update)
		psr_enable_request = true;
	else if (active_psr_events & psr_event_vsync)
		psr_enable_request = false;
	else if (active_psr_events & psr_event_crc_window_active)
		psr_enable_request = false;
	else
		psr_enable_request = true;

	DC_TRACE_LEVEL_MESSAGE(DAL_TRACE_LEVEL_VERBOSE,
						WPP_BIT_FLAG_Firmware_PsrState,
						"mod_power set_psr_event: before: psr_enabled=%d -> request: set_event=%d event=0x%04x -> result: psr_events=0x%04x psr_enable_request=%d",
						(int)core_power->map[stream_index].psr_enabled,
						(int)set_event,
						(unsigned int)event,
						(unsigned int)core_power->map[stream_index].psr_events,
						(int)psr_enable_request);
	mod_power_psr_set_power_opt(mod_power, stream, active_psr_events, psr_enable_request);

	if (core_power->map[stream_index].psr_enabled != psr_enable_request || force_static) {
		if (set_psr_enable(mod_power, stream, psr_enable_request, wait, force_static)) {
			core_power->map[stream_index].psr_enabled = psr_enable_request;
		}
	}

	return true;
}

bool mod_power_get_psr_state(struct mod_power *mod_power,
		const struct dc_stream_state *stream,
		enum dc_psr_state *state)
{
	struct core_power *core_power = NULL;
	const struct dc_link *link = NULL;

	if (!stream)
		return false;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	link = dc_stream_get_link(stream);
	return dc_link_get_psr_state(link, state);
}

bool mod_power_get_psr_enabled_status(struct mod_power *mod_power,
		const struct dc_stream_state *stream,
		bool *psr_enabled)
{
	struct core_power *core_power = NULL;
	unsigned int stream_index = 0;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	stream_index = map_index_from_stream(core_power, stream);

	if (!core_power->map[stream_index].caps->psr_version)
		return false;

	*psr_enabled = core_power->map[stream_index].psr_enabled;

	return true;
}

void mod_power_psr_residency(struct mod_power *mod_power,
		const struct dc_stream_state *stream,
		unsigned int *residency,
		const uint8_t mode)
{
	struct core_power *core_power = NULL;
	const struct dc_link *link = NULL;

	if (!stream)
		return;

	if (mod_power == NULL)
		return;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return;

	link = dc_stream_get_link(stream);

	if (link != NULL)
		link->dc->link_srv->edp_get_psr_residency(link, residency, mode);
}
bool mod_power_psr_get_active_psr_events(struct mod_power *mod_power,
		const struct dc_stream_state *stream, unsigned int *active_psr_events)
{
	struct core_power *core_power = NULL;
	unsigned int stream_index = 0;

	if (!stream)
		return false;

	if (mod_power == NULL)
		return false;

	if (active_psr_events == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	stream_index = map_index_from_stream(core_power, stream);

	*active_psr_events = core_power->map[stream_index].psr_events;
	return true;
}

bool mod_power_psr_set_sink_vtotal_in_psr_active(struct mod_power *mod_power,
		const struct dc_stream_state *stream,
		uint16_t psr_vtotal_idle,
		uint16_t psr_vtotal_su)
{
	struct core_power *core_power = NULL;
	unsigned int stream_index = 0;
	const struct dc_link *link = NULL;

	if (!stream)
		return false;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	stream_index = map_index_from_stream(core_power, stream);

	if (!core_power->map[stream_index].caps->psr_version)
		return false;

	link = dc_stream_get_link(stream);

	return link->dc->link_srv->edp_set_sink_vtotal_in_psr_active(
			link, psr_vtotal_idle, psr_vtotal_su);
}

static bool mod_power_set_replay_active(struct dc_stream_state *stream,
	bool replay_active,
	bool wait,
	bool force_static)
{
	uint64_t state;
	unsigned int retry_count;
	const unsigned int max_retry = 1000;
	struct dc_link *link = NULL;

	if (!stream)
		return false;

	link = dc_stream_get_link(stream);

	if (!link)
		return false;

	if (!dc_link_set_replay_allow_active(link, &replay_active, false, force_static, NULL))
		return false;

	if (wait == true) {

		for (retry_count = 0; retry_count <= max_retry; retry_count++) {
			dc_link_get_replay_state(link, &state);
			if (replay_active) {
				if (state != REPLAY_STATE_0 &&
					(!force_static || state == REPLAY_STATE_3))
					break;
			} else {
				if (state == REPLAY_STATE_0)
					break;
			}
			udelay(500);
		}

		/* assert if max retry hit */
		if (retry_count >= max_retry)
			ASSERT(0);
	} else {
		/* To-do: Add trace log */
	}

	return true;
}

static unsigned int mod_power_replay_setup_power_opt(struct dc_link *link,
	unsigned int active_replay_events, bool is_ultra_sleep_mode)
{
	unsigned int power_opt = 0;

	if (is_ultra_sleep_mode) {
		/* Static Screen */
		power_opt |= (replay_power_opt_smu_opt_static_screen | replay_power_opt_z10_static_screen);
	} else if (active_replay_events & replay_event_test_harness_ultra_sleep) {
		power_opt |= replay_power_opt_z10_static_screen;
	}

	/* replay_power_opt_flag is a configuration parameter into the module that determines
	 * which optimizations to enable during replay
	 */
	power_opt &= link->replay_settings.config.replay_power_opt_supported;

	return power_opt;
}

static bool mod_power_replay_set_power_opt(struct mod_power *mod_power,
	struct dc_stream_state *stream,
	unsigned int active_replay_events,
	bool is_ultra_sleep_mode)
{
	(void)mod_power;
	struct dc_link *link = NULL;
	unsigned int power_opt = 0;

	if (!stream)
		return false;

	link = dc_stream_get_link(stream);

	if (!link || !link->replay_settings.replay_feature_enabled)
		return false;

	power_opt = mod_power_replay_setup_power_opt(link, active_replay_events, is_ultra_sleep_mode);

	if (!dc_link_set_replay_allow_active(link, NULL, false, false, &power_opt))
		return false;

	return true;
}

bool mod_power_get_replay_event(struct mod_power *mod_power,
	struct dc_stream_state *stream,
	unsigned int *active_replay_events)
{
	struct core_power *core_power = NULL;
	unsigned int stream_index = 0;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	stream_index = map_index_from_stream(core_power, stream);

	*active_replay_events = core_power->map[stream_index].replay_events;

	return true;
}

static bool mod_power_update_replay_active_status(unsigned int active_replay_events,
	struct dc_link *link, uint32_t *coasting_vtotal, bool *is_full_screen_video, bool *is_ultra_sleep_mode, uint16_t *frame_skip_number, bool *is_video_playback)
{
	if (!link || !coasting_vtotal || !is_full_screen_video || !is_video_playback)
		return false;

	// Check coasting_vtotal_table has been updated.
	if (!link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_STATIC]
		|| !link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_NOM])
		return false;

	unsigned int replay_enable_option =
		link->replay_settings.config.replay_enable_option;

	/* TODO: To support test harness and DDS event */

	*coasting_vtotal = link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_NOM];
	ASSERT(link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_NOM] <= 0xFFFF);
	*frame_skip_number = (uint16_t)link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_NOM];

	link->replay_settings.config.replay_timing_sync_supported = false;

	*is_full_screen_video = false;

	*is_ultra_sleep_mode = false;

	*is_video_playback = false;

	/* DSAT test scenario */
	if (active_replay_events & replay_event_test_harness_mode) {
		if (link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_TEST_HARNESS])
			*coasting_vtotal =
				link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_TEST_HARNESS];
		if (link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_TEST_HARNESS]) {
			ASSERT(link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_TEST_HARNESS] <= 0xFFFF);
			*frame_skip_number =
				(uint16_t)link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_TEST_HARNESS];
		}

		/* During the ultra sleep mode testing, disable the timing sync in short vblank mode */
		if (active_replay_events & (replay_event_test_harness_enable_replay)) {
			if ((active_replay_events & replay_event_test_harness_ultra_sleep) &&
				  !link->replay_settings.config.replay_support_fast_resync_in_ultra_sleep_mode)
				link->replay_settings.config.replay_timing_sync_supported = false;
			return true;
		} else
			return false;
	} else if (active_replay_events & (replay_event_test_harness_enable_replay)) {
		if (link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_TEST_HARNESS])
			*coasting_vtotal = link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_TEST_HARNESS];
		if (link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_TEST_HARNESS]) {
			uint32_t frame_skip_val =
				link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_TEST_HARNESS];

			ASSERT(frame_skip_val <= 0xFFFF);
			*frame_skip_number = (uint16_t)frame_skip_val;
		}

		/* During the ultra sleep mode testing, disable the timing sync in short vblank mode */
		if ((active_replay_events & replay_event_test_harness_ultra_sleep) &&
			  !link->replay_settings.config.replay_support_fast_resync_in_ultra_sleep_mode)
			link->replay_settings.config.replay_timing_sync_supported = false;
		return true;
	} else if (active_replay_events & (replay_event_test_harness_disable_replay | replay_event_os_request_disable)) {
		// set last set coasting vtotal
		if (link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_TEST_HARNESS])
			*coasting_vtotal = link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_TEST_HARNESS];
		if (link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_TEST_HARNESS]) {
			uint32_t frame_skip_val =
				link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_TEST_HARNESS];

			ASSERT(frame_skip_val <= 0xFFFF);
			*frame_skip_number = (uint16_t)frame_skip_val;
		}
		return false;
	}

	/* Inactive conditions */
	if (active_replay_events & (replay_event_edp_panel_off_disable_psr |
			replay_event_hw_programming |
			replay_event_vrr |
			replay_event_immediate_flip |
			replay_event_prepare_vtotal |
			replay_event_vrr_transition |
			replay_event_pause |
			replay_event_disable_replay_while_DPMS |
			replay_event_sleep_resume |
			replay_event_disable_in_AC |
			replay_event_disable_replay_while_detect_display |
			replay_event_infopacket |
			replay_event_crc_window_active))
		return false;

	// Full screen scenario
	if (active_replay_events & replay_event_full_screen) {
		if (!(replay_enable_option & pr_enable_option_full_screen))
			return false;
	}

	/* Full screen video scenario */
	if (active_replay_events & replay_event_big_screen_video) {

		link->replay_settings.config.replay_timing_sync_supported = false;

		if (replay_enable_option & pr_enable_option_full_screen_video_coasting) {
			unsigned int fsn_vid =
				link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_FULL_SCREEN_VIDEO];

			*coasting_vtotal =
				link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_FULL_SCREEN_VIDEO];
			ASSERT(fsn_vid <= 0xFFFF);
			*frame_skip_number = (uint16_t)fsn_vid;
		}

		*is_video_playback = true;

		if ((replay_enable_option & pr_enable_option_full_screen_video) &&
			(replay_enable_option & pr_enable_option_full_screen_video_coasting)) {
			*is_full_screen_video = true;
			return true;
		} else
			return false;
	}

	/* MPO video scenario
	 * Some of the cases may contain a full screen UI layer in MPO video scenario which is
	 * not the expected case to enable Replay.
	 */
	if ((active_replay_events & replay_event_mpo_video_selective_update) &&
		!(active_replay_events & replay_event_full_screen)) {

		link->replay_settings.config.replay_timing_sync_supported = false;

		if (replay_enable_option & pr_enable_option_mpo_video_coasting) {
			*coasting_vtotal = link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_NOM];
			{
				uint32_t frame_skip_val =
					link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_NOM];

				ASSERT(frame_skip_val <= 0xFFFF);
				*frame_skip_number = (uint16_t)frame_skip_val;
			}
		}

		*is_video_playback = true;

		if (replay_enable_option & pr_enable_option_mpo_video)
			return true;
		else
			return false;
	}

	/* Static screen scenario */
	if (!(active_replay_events & replay_event_vsync)) {

		if (replay_enable_option & pr_enable_option_static_screen_coasting) {
			// Do not adjust eDP refresh rate if static screen + normal sleep mode
			if ((!(link->replay_settings.config.replay_power_opt_supported &
				replay_power_opt_z10_static_screen)) ||
				(active_replay_events & replay_event_cursor_updating)) {
				// normal sleep mode
				*coasting_vtotal =
					link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_NOM];
				{
					uint32_t frame_skip_val =
						link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_NOM];

					ASSERT(frame_skip_val <= 0xFFFF);
					*frame_skip_number = (uint16_t)frame_skip_val;
				}
			} else {
				// ultra sleep mode
				*coasting_vtotal =
					link->replay_settings.coasting_vtotal_table[PR_COASTING_TYPE_STATIC];
				{
					uint32_t frame_skip_val =
						link->replay_settings.frame_skip_number_table[PR_COASTING_TYPE_STATIC];

					ASSERT(frame_skip_val <= 0xFFFF);
					*frame_skip_number = (uint16_t)frame_skip_val;
				}
				*is_ultra_sleep_mode = true;
			}
		}

		if (replay_enable_option & pr_enable_option_static_screen) {
			if (!link->replay_settings.config.replay_support_fast_resync_in_ultra_sleep_mode)
				link->replay_settings.config.replay_timing_sync_supported = false;
			return true;
		} else
			return false;
	}

	/* General UI scenario */
	if (active_replay_events & replay_event_general_ui) {
		if (replay_enable_option & pr_enable_option_general_ui)
			return true;
		else
			return false;
	}

	return false;
}

bool mod_power_replay_set_coasting_vtotal(struct mod_power *mod_power,
	const struct dc_stream_state *stream,
	uint32_t coasting_vtotal,
	uint16_t frame_skip_number)
{
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;

	if (!stream)
		return false;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return false;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	return link->dc->link_srv->edp_set_coasting_vtotal(link, coasting_vtotal, frame_skip_number);
}

void mod_power_replay_set_timing_sync_supported(struct mod_power *mod_power,
	const struct dc_stream_state *stream)
{
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	unsigned int stream_index = 0;
	union dmub_replay_cmd_set cmd_data = { 0 };

	if (!stream || mod_power == NULL)
		return;

	core_power = MOD_POWER_TO_CORE(mod_power);
	if (core_power->num_entities == 0)
		return;

	stream_index = map_index_from_stream(core_power, stream);
	if (stream_index > core_power->num_entities) //invalid index
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	cmd_data.sync_data.timing_sync_supported = link->replay_settings.config.replay_timing_sync_supported;

	link->dc->link_srv->edp_send_replay_cmd(link, Replay_Set_Timing_Sync_Supported,
		&cmd_data);
}

void mod_power_replay_disabled_adaptive_sync_sdp(struct mod_power *mod_power,
	const struct dc_stream_state *stream, bool force_disabled)
{
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	unsigned int stream_index = 0;
	union dmub_replay_cmd_set cmd_data = { 0 };

	if (!stream || mod_power == NULL)
		return;

	core_power = MOD_POWER_TO_CORE(mod_power);
	if (core_power->num_entities == 0)
		return;

	stream_index = map_index_from_stream(core_power, stream);
	if (stream_index > core_power->num_entities) //invalid index
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	cmd_data.disabled_adaptive_sync_sdp_data.force_disabled = force_disabled;

	link->dc->link_srv->edp_send_replay_cmd(link, Replay_Disabled_Adaptive_Sync_SDP,
		&cmd_data);
}

static void mod_power_replay_set_general_cmd(struct mod_power *mod_power,
	const struct dc_stream_state *stream,
	const enum dmub_cmd_replay_general_subtype general_cmd_type,
	const uint32_t param1, const uint32_t param2)
{
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	unsigned int stream_index = 0;
	union dmub_replay_cmd_set cmd_data = { 0 };

	if (!stream || mod_power == NULL)
		return;

	core_power = MOD_POWER_TO_CORE(mod_power);
	if (core_power->num_entities == 0)
		return;

	stream_index = map_index_from_stream(core_power, stream);
	if (stream_index > core_power->num_entities) //invalid index
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	cmd_data.set_general_cmd_data.subtype = general_cmd_type;
	cmd_data.set_general_cmd_data.param1 = param1;
	cmd_data.set_general_cmd_data.param2 = param2;
	link->dc->link_srv->edp_send_replay_cmd(link, Replay_Set_General_Cmd,
		&cmd_data);
}

void mod_power_replay_disabled_desync_error_detection(struct mod_power *mod_power,
	const struct dc_stream_state *stream,  bool force_disabled)
{
	mod_power_replay_set_general_cmd(mod_power, stream,
			REPLAY_GENERAL_CMD_DISABLED_DESYNC_ERROR_DETECTION,
			force_disabled, 0);
}

static void mod_power_replay_set_pseudo_vtotal(struct mod_power *mod_power,
	const struct dc_stream_state *stream, uint16_t vtotal)
{
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	unsigned int stream_index = 0;
	union dmub_replay_cmd_set cmd_data = { 0 };

	if (!stream || mod_power == NULL)
		return;

	core_power = MOD_POWER_TO_CORE(mod_power);
	if (core_power->num_entities == 0)
		return;

	stream_index = map_index_from_stream(core_power, stream);
	if (stream_index > core_power->num_entities) //invalid index
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	cmd_data.pseudo_vtotal_data.vtotal = vtotal;

	if (link->replay_settings.last_pseudo_vtotal != vtotal) {
		link->replay_settings.last_pseudo_vtotal = vtotal;
		link->dc->link_srv->edp_send_replay_cmd(link, Replay_Set_Pseudo_VTotal, &cmd_data);
	}
}

static void mod_power_update_error_status(struct mod_power *mod_power,
	const struct dc_stream_state *stream)
{
	struct dc_link *link = NULL;
	union replay_debug_flags *pDebug = NULL;

	if (mod_power == NULL || stream == NULL)
		return;

	link = dc_stream_get_link(stream);

	if (!link)
		return;

	pDebug = (union replay_debug_flags *)&link->replay_settings.config.debug_flags;

	if (0 == pDebug->bitfields.enable_visual_confirm_debug)
		return;

	mod_power_replay_set_general_cmd(mod_power, stream,
		REPLAY_GENERAL_CMD_UPDATE_ERROR_STATUS,
		link->replay_settings.config.replay_error_status.raw, 0);
}

void mod_power_set_low_rr_activate(struct mod_power *mod_power,
	const struct dc_stream_state *stream, bool low_rr_supported)
{
	struct dc_link *link = NULL;

	if (mod_power == NULL || stream == NULL)
		return;

	link = dc_stream_get_link(stream);

	if (!link)
		return;

	mod_power_replay_set_general_cmd(mod_power, stream,
		REPLAY_GENERAL_CMD_SET_LOW_RR_ACTIVATE,
		low_rr_supported, 0);
}

void mod_power_set_video_conferencing_activate(struct mod_power *mod_power,
	const struct dc_stream_state *stream, bool video_conferencing_activate)
{
	struct dc_link *link = NULL;

	if (mod_power == NULL || stream == NULL)
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	mod_power_replay_set_general_cmd(mod_power, stream,
		REPLAY_GENERAL_CMD_VIDEO_CONFERENCING,
		video_conferencing_activate, 0);
}

void mod_power_set_coasting_vtotal_without_frame_update(struct mod_power *mod_power,
	const struct dc_stream_state *stream, uint32_t coasting_vtotal)
{
	struct dc_link *link = NULL;

	if (mod_power == NULL || stream == NULL)
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	mod_power_replay_set_general_cmd(mod_power, stream,
		REPLAY_GENERAL_CMD_SET_COASTING_VTOTAL_WITHOUT_FRAME_UPDATE,
		coasting_vtotal, 0);
}

void mod_power_set_replay_continuously_resync(struct mod_power *mod_power,
	const struct dc_stream_state *stream, bool enable)
{
	struct dc_link *link = NULL;

	if (mod_power == NULL || stream == NULL)
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	mod_power_replay_set_general_cmd(mod_power, stream,
		REPLAY_GENERAL_CMD_SET_CONTINUOUSLY_RESYNC,
		enable, 0);
}

void mod_power_set_live_capture_with_cvt_activate(struct mod_power *mod_power,
	const struct dc_stream_state *stream, bool live_capture_with_cvt_activate)
{
	struct dc_link *link = NULL;

	if (mod_power == NULL || stream == NULL)
		return;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return;

	// Check if LIVE_CAPTURE_WITH_CVT bit is enabled in DalRegKey_ReplayOptimization
	if (!link->replay_settings.config.replay_optimization.bits.LIVE_CAPTURE_WITH_CVT)
		return;

	if (link->replay_settings.config.live_capture_with_cvt_activated != live_capture_with_cvt_activate) {
		link->replay_settings.config.live_capture_with_cvt_activated = live_capture_with_cvt_activate;
		mod_power_replay_set_general_cmd(mod_power, stream,
			REPLAY_GENERAL_CMD_LIVE_CAPTURE_WITH_CVT,
			live_capture_with_cvt_activate, 0);
	}
}

bool mod_power_set_replay_event(struct mod_power *mod_power,
	struct dc_stream_state *stream, bool set_event,
	enum replay_event event, bool wait_for_disable)
{
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	unsigned int stream_index = 0;
	unsigned int active_replay_events = 0;
	bool replay_active_request = false;
	bool force_static = false;
	uint32_t coasting_vtotal = 0;
	bool current_timing_sync_status = false;
	bool is_full_screen_video = false;
	bool is_ultra_sleep_mode = false;
	unsigned int sink_duration_us = 0;
	bool low_rr_active = false;
	uint16_t frame_skip_number = 0;
	bool is_video_playback = false;

	if (!stream)
		return false;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	stream_index = map_index_from_stream(core_power, stream);

	if (set_event)
		core_power->map[stream_index].replay_events |= event;
	else
		core_power->map[stream_index].replay_events &= ~event;

	link = dc_stream_get_link(stream);
	if (!link || !link->replay_settings.replay_feature_enabled)
		return false;

	if ((core_power->map[stream_index].replay_events & replay_event_disable_replay_while_switching_mux) != 0)
		return false;

	if ((core_power->map[stream_index].replay_events & replay_event_os_override_hold) != 0)
		return false;

	active_replay_events = core_power->map[stream_index].replay_events;

	current_timing_sync_status =
		link->replay_settings.config.replay_timing_sync_supported;

	replay_active_request = mod_power_update_replay_active_status(active_replay_events,
		link, &coasting_vtotal, &is_full_screen_video, &is_ultra_sleep_mode, &frame_skip_number, &is_video_playback);

	if (is_full_screen_video)
		mod_power_replay_set_pseudo_vtotal(mod_power, stream,
			link->replay_settings.low_rr_full_screen_video_pseudo_vtotal);
	else
		mod_power_replay_set_pseudo_vtotal(mod_power, stream, 0);

	//If timing_sync_status change, then re-enabled set timing_sync_supported value and re-enabled replay
	if (current_timing_sync_status != link->replay_settings.config.replay_timing_sync_supported)
		mod_power_replay_set_timing_sync_supported(mod_power, stream);

	if (link->replay_settings.config.low_rr_supported) {
		sink_duration_us =
			(unsigned int)(div_u64(((unsigned long long)(coasting_vtotal)
				* 10000) * stream->timing.h_total,
					stream->timing.pix_clk_100hz));
		low_rr_active = sink_duration_us < LOW_REFRESH_RATE_DURATION_US_UPPER_BOUND ? false : true;
		if (low_rr_active != link->replay_settings.config.low_rr_activated) {
			mod_power_set_low_rr_activate(mod_power, stream, low_rr_active);
			link->replay_settings.config.low_rr_activated = low_rr_active;
		}
	}

	// The function return fail when
	// 1. DMUB function is not support (for backward compatible).
	// 2. active_replay_events or coasting_vtotal is not updated in the same time
	if (!mod_power_replay_set_power_opt_and_coasting_vtotal(mod_power,
		stream, active_replay_events, coasting_vtotal, is_ultra_sleep_mode, frame_skip_number)) {
		if (!mod_power_replay_set_power_opt(mod_power, stream, active_replay_events, is_ultra_sleep_mode))
			return false;

		if (!mod_power_replay_set_coasting_vtotal(mod_power, stream, coasting_vtotal, frame_skip_number))
			return false;
	}

	mod_power_set_live_capture_with_cvt_activate(mod_power, stream, is_video_playback);

	mod_power_update_error_status(mod_power, stream);

	// If Replay is going to be enable (No matter is disable -> enable or enable -> enable), we don't need to wait.
	// If Replay is going to be disable
	//     if disable -> disable
	//         -> Replay DMUB state should be state 0.
	//            So no matter wait_for_disable is true or not, it should makes no difference.
	//     if enable -> disable -> We should wait if wait_for_disable is true.
	if (replay_active_request)
		wait_for_disable = false;

	if (!mod_power_set_replay_active(stream, replay_active_request, wait_for_disable, force_static))
		return false;

	return true;
}

bool mod_power_get_replay_active_status(const struct dc_stream_state *stream,
	bool *replay_active)
{
	const struct dc_link *link = NULL;

	if (!stream)
		return false;

	link = dc_stream_get_link(stream);
	*replay_active = link->replay_settings.replay_allow_active;

	return true;
}

void mod_power_replay_residency(const struct dc_stream_state *stream,
	unsigned int *residency, const bool is_start, const bool is_alpm)
{
	const struct dc_link *link = NULL;
	enum pr_residency_mode mode;

	if (!stream)
		return;

	link = dc_stream_get_link(stream);

	if (is_alpm)
		mode = PR_RESIDENCY_MODE_ALPM;
	else
		mode = PR_RESIDENCY_MODE_PHY;

	if (link && link->dc && link->dc->link_srv)
		link->dc->link_srv->edp_replay_residency(link, residency, is_start, mode);
}

bool mod_power_replay_set_power_opt_and_coasting_vtotal(struct mod_power *mod_power,
	const struct dc_stream_state *stream, unsigned int active_replay_events, uint32_t coasting_vtotal,
	bool is_ultra_sleep_mode, uint16_t frame_skip_number)
{
	struct core_power *core_power = NULL;
	struct dc_link *link = NULL;
	unsigned int power_opt = 0;

	if (!stream)
		return false;

	if (mod_power == NULL)
		return false;

	core_power = MOD_POWER_TO_CORE(mod_power);

	if (core_power->num_entities == 0)
		return false;

	link = dc_stream_get_link(stream);

	if (!link || !link->replay_settings.replay_feature_enabled)
		return false;

	power_opt = mod_power_replay_setup_power_opt(link, active_replay_events, is_ultra_sleep_mode);

	return link->dc->link_srv->edp_set_replay_power_opt_and_coasting_vtotal(link, &power_opt, coasting_vtotal, frame_skip_number);
}





