/* Copyright 2018 Advanced Micro Devices, Inc.
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

#include "power_helpers.h"
#include "dc/inc/hw/dmcu.h"
#include "dc/inc/hw/abm.h"
#include "dc.h"
#include "core_types.h"
#include "dmub_cmd.h"

#define DIV_ROUNDUP(a, b) (((a)+((b)/2))/(b))
#define bswap16_based_on_endian(big_endian, value) \
	((big_endian) ? cpu_to_be16(value) : cpu_to_le16(value))

/*
 * is_psr_su_specific_panel() - check if sink is AMD vendor-specific PSR-SU
 * supported eDP device.
 *
 * @link: dc link pointer
 *
 * Return: true if AMDGPU vendor specific PSR-SU eDP panel
 */
bool is_psr_su_specific_panel(struct dc_link *link)
{
	bool isPSRSUSupported = false;
	struct dpcd_caps *dpcd_caps = &link->dpcd_caps;

	if (dpcd_caps->edp_rev >= DP_EDP_14) {
		if (dpcd_caps->psr_info.psr_version >= DP_PSR2_WITH_Y_COORD_ET_SUPPORTED)
			isPSRSUSupported = true;
		/*
		 * Some panels will report PSR capabilities over additional DPCD bits.
		 * Such panels are approved despite reporting only PSR v3, as long as
		 * the additional bits are reported.
		 */
		if (dpcd_caps->sink_dev_id == DP_BRANCH_DEVICE_ID_001CF8) {
			/*
			 * This is the temporary workaround to disable PSRSU when system turned on
			 * DSC function on the sepcific sink.
			 */
			if (dpcd_caps->psr_info.psr_version < DP_PSR2_WITH_Y_COORD_IS_SUPPORTED)
				isPSRSUSupported = false;
			else if (dpcd_caps->dsc_caps.dsc_basic_caps.fields.dsc_support.DSC_SUPPORT &&
				((dpcd_caps->sink_dev_id_str[1] == 0x08 && dpcd_caps->sink_dev_id_str[0] == 0x08) ||
				(dpcd_caps->sink_dev_id_str[1] == 0x08 && dpcd_caps->sink_dev_id_str[0] == 0x07)))
				isPSRSUSupported = false;
			else if (dpcd_caps->sink_dev_id_str[1] == 0x08 && dpcd_caps->sink_dev_id_str[0] == 0x03)
				isPSRSUSupported = false;
			else if (dpcd_caps->sink_dev_id_str[1] == 0x08 && dpcd_caps->sink_dev_id_str[0] == 0x01)
				isPSRSUSupported = false;
			else if (dpcd_caps->psr_info.force_psrsu_cap == 0x1)
				isPSRSUSupported = true;
		}
	}

	return isPSRSUSupported;
}

/**
 * mod_power_calc_psr_configs() - calculate/update generic psr configuration fields.
 * @psr_config: [output], psr configuration structure to be updated
 * @link: [input] dc link pointer
 * @stream: [input] dc stream state pointer
 *
 * calculate and update the psr configuration fields that are not DM specific, i.e. such
 * fields which are based on DPCD caps or timing information. To setup PSR in DMUB FW,
 * this helper is assumed to be called before the call of the DC helper dc_link_setup_psr().
 *
 * PSR config fields to be updated within the helper:
 * - psr_rfb_setup_time
 * - psr_sdp_transmit_line_num_deadline
 * - line_time_in_us
 * - su_y_granularity
 * - su_granularity_required
 * - psr_frame_capture_indication_req
 * - psr_exit_link_training_required
 *
 * PSR config fields that are DM specific and NOT updated within the helper:
 * - allow_smu_optimizations
 * - allow_multi_disp_optimizations
 */
void mod_power_calc_psr_configs(struct psr_config *psr_config,
		struct dc_link *link,
		const struct dc_stream_state *stream)
{
	unsigned int num_vblank_lines = 0;
	unsigned int vblank_time_in_us = 0;
	unsigned int sdp_tx_deadline_in_us = 0;
	unsigned int line_time_in_us = 0;
	struct dpcd_caps *dpcd_caps = &link->dpcd_caps;
	const int psr_setup_time_step_in_us = 55;	/* refer to eDP spec DPCD 0x071h */

	/* timing parameters */
	num_vblank_lines = stream->timing.v_total -
			 stream->timing.v_addressable -
			 stream->timing.v_border_top -
			 stream->timing.v_border_bottom;

	vblank_time_in_us = (stream->timing.h_total * num_vblank_lines * 1000) / (stream->timing.pix_clk_100hz / 10);

	line_time_in_us = ((stream->timing.h_total * 1000) / (stream->timing.pix_clk_100hz / 10)) + 1;

	/**
	 * psr configuration fields
	 *
	 * as per eDP 1.5 pg. 377 of 459, DPCD 0x071h bits [3:1], psr setup time bits interpreted as below
	 * 000b <--> 330 us (default)
	 * 001b <--> 275 us
	 * 010b <--> 220 us
	 * 011b <--> 165 us
	 * 100b <--> 110 us
	 * 101b <--> 055 us
	 * 110b <--> 000 us
	 */
	psr_config->psr_rfb_setup_time =
		(6 - dpcd_caps->psr_info.psr_dpcd_caps.bits.PSR_SETUP_TIME) * psr_setup_time_step_in_us;

	if (psr_config->psr_rfb_setup_time > vblank_time_in_us) {
		link->psr_settings.psr_frame_capture_indication_req = true;
		link->psr_settings.psr_sdp_transmit_line_num_deadline = num_vblank_lines;
	} else {
		sdp_tx_deadline_in_us = vblank_time_in_us - psr_config->psr_rfb_setup_time;

		/* Set the last possible line SDP may be transmitted without violating the RFB setup time */
		link->psr_settings.psr_frame_capture_indication_req = false;
		link->psr_settings.psr_sdp_transmit_line_num_deadline = sdp_tx_deadline_in_us / line_time_in_us;
	}

	psr_config->psr_sdp_transmit_line_num_deadline = link->psr_settings.psr_sdp_transmit_line_num_deadline;
	psr_config->line_time_in_us = line_time_in_us;
	psr_config->su_y_granularity = dpcd_caps->psr_info.psr2_su_y_granularity_cap;
	psr_config->su_granularity_required = dpcd_caps->psr_info.psr_dpcd_caps.bits.SU_GRANULARITY_REQUIRED;
	psr_config->psr_frame_capture_indication_req = link->psr_settings.psr_frame_capture_indication_req;
	psr_config->psr_exit_link_training_required =
		!link->dpcd_caps.psr_info.psr_dpcd_caps.bits.LINK_TRAINING_ON_EXIT_NOT_REQUIRED;
}

void init_replay_config(struct dc_link *link, struct replay_config *pr_config)
{
	link->replay_settings.config = *pr_config;
}

bool mod_power_only_edp(const struct dc_state *context, const struct dc_stream_state *stream)
{
	return context && context->stream_count == 1 && dc_is_embedded_signal(stream->signal);
}

bool psr_su_set_dsc_slice_height(struct dc *dc, struct dc_link *link,
			      struct dc_stream_state *stream,
			      struct psr_config *config)
{
	uint32_t pic_height;
	uint32_t slice_height;

	config->dsc_slice_height = 0;
	if (!(link->connector_signal & SIGNAL_TYPE_EDP) ||
	    !dc->caps.edp_dsc_support ||
	    link->panel_config.dsc.disable_dsc_edp ||
	    !link->dpcd_caps.dsc_caps.dsc_basic_caps.fields.dsc_support.DSC_SUPPORT ||
	    !stream->timing.dsc_cfg.num_slices_v)
		return true;

	pic_height = stream->timing.v_addressable +
		stream->timing.v_border_top + stream->timing.v_border_bottom;

	if (stream->timing.dsc_cfg.num_slices_v == 0)
		return false;

	slice_height = pic_height / stream->timing.dsc_cfg.num_slices_v;
	config->dsc_slice_height = (uint16_t)slice_height;

	if (slice_height) {
		if (config->su_y_granularity &&
		    (slice_height % config->su_y_granularity)) {
			ASSERT(0);
			return false;
		}
	}

	return true;
}

void set_replay_frame_skip_number(struct dc_link *link,
	enum replay_coasting_vtotal_type type,
	uint32_t coasting_vtotal_refresh_rate_uhz,
	uint32_t flicker_free_refresh_rate_uhz,
	bool is_defer)
{
	uint32_t *frame_skip_number_array = NULL;
	uint32_t frame_skip_number = 0;

	if (link == NULL)
		return;

	if (false == link->replay_settings.config.frame_skip_supported)
		return;

	if (flicker_free_refresh_rate_uhz == 0 || coasting_vtotal_refresh_rate_uhz == 0)
		return;

	if (is_defer)
		frame_skip_number_array = link->replay_settings.defer_frame_skip_number_table;
	else
		frame_skip_number_array = link->replay_settings.frame_skip_number_table;

	if (frame_skip_number_array == NULL)
		return;

	frame_skip_number = (coasting_vtotal_refresh_rate_uhz + 500000) / flicker_free_refresh_rate_uhz;

	if (frame_skip_number >= 1)
		frame_skip_number_array[type] = frame_skip_number - 1;
	else
		frame_skip_number_array[type] = 0;
}

void set_replay_defer_update_coasting_vtotal(struct dc_link *link,
	enum replay_coasting_vtotal_type type,
	uint32_t vtotal)
{
	link->replay_settings.defer_update_coasting_vtotal_table[type] = vtotal;
}

void update_replay_coasting_vtotal_from_defer(struct dc_link *link,
	enum replay_coasting_vtotal_type type)
{
	link->replay_settings.coasting_vtotal_table[type] =
		link->replay_settings.defer_update_coasting_vtotal_table[type];
	link->replay_settings.frame_skip_number_table[type] =
		link->replay_settings.defer_frame_skip_number_table[type];
}

void set_replay_coasting_vtotal(struct dc_link *link,
	enum replay_coasting_vtotal_type type,
	uint32_t vtotal)
{
	link->replay_settings.coasting_vtotal_table[type] = vtotal;
}

void set_replay_low_rr_full_screen_video_src_vtotal(struct dc_link *link, uint16_t vtotal)
{
	link->replay_settings.low_rr_full_screen_video_pseudo_vtotal = vtotal;
}

void calculate_replay_link_off_frame_count(struct dc_link *link,
	uint16_t vtotal, uint16_t htotal)
{
	uint32_t max_link_off_frame_count = 0;
	uint16_t max_deviation_line = 0,  pixel_deviation_per_line = 0;

	if (!link || link->replay_settings.config.replay_version != DC_FREESYNC_REPLAY)
		return;

	max_deviation_line = link->dpcd_caps.pr_info.max_deviation_line;
	pixel_deviation_per_line = link->dpcd_caps.pr_info.pixel_deviation_per_line;

	if (htotal != 0 && vtotal != 0 && pixel_deviation_per_line != 0)
		max_link_off_frame_count = htotal * max_deviation_line / (pixel_deviation_per_line * vtotal);
	else
		ASSERT(0);

	link->replay_settings.link_off_frame_count = max_link_off_frame_count;
}

void reset_replay_dsync_error_count(struct dc_link *link)
{
	link->replay_settings.replay_desync_error_fail_count = 0;
}
