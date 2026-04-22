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

void init_replay_config(struct dc_link *link, struct replay_config *pr_config)
{
	link->replay_settings.config = *pr_config;
}

bool mod_power_only_edp(const struct dc_state *context, const struct dc_stream_state *stream)
{
	return context && context->stream_count == 1 && dc_is_embedded_signal(stream->signal);
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
