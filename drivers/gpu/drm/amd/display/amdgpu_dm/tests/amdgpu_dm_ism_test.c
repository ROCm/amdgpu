// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_ism.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>

#include "dc.h"
#include "amdgpu_dm_ism.h"

/*
 * Helper: allocate and zero-initialise a dc_stream_state for timing tests.
 * Only the timing sub-struct is accessed by the functions under test.
 */
static struct dc_stream_state *alloc_test_stream(struct kunit *test)
{
	struct dc_stream_state *stream;

	stream = kunit_kzalloc(test, sizeof(*stream), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, stream);

	return stream;
}

/*
 * Helper: allocate and zero-initialise an ISM instance.
 */
static struct amdgpu_dm_ism *alloc_test_ism(struct kunit *test)
{
	struct amdgpu_dm_ism *ism;

	ism = kunit_kzalloc(test, sizeof(*ism), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ism);

	return ism;
}

/* ===== Tests for dm_ism_next_state — FULL_POWER_RUNNING transitions ===== */

static void dm_test_ism_next_state_running_enter_idle(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_FULL_POWER_RUNNING,
			       DM_ISM_EVENT_ENTER_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_HYSTERESIS_WAITING);
}

static void dm_test_ism_next_state_running_begin_cursor(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_FULL_POWER_RUNNING,
			       DM_ISM_EVENT_BEGIN_CURSOR_UPDATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_FULL_POWER_BUSY);
}

static void dm_test_ism_next_state_running_invalid(struct kunit *test)
{
	enum amdgpu_dm_ism_state next = DM_ISM_NUM_STATES;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_FULL_POWER_RUNNING,
			       DM_ISM_EVENT_EXIT_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_FALSE(test, ok);
	/* next should remain untouched on invalid transition */
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_NUM_STATES);
}

/* ===== Tests for dm_ism_next_state — FULL_POWER_BUSY transitions ===== */

static void dm_test_ism_next_state_busy_enter_idle(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_FULL_POWER_BUSY,
			       DM_ISM_EVENT_ENTER_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_HYSTERESIS_BUSY);
}

static void dm_test_ism_next_state_busy_end_cursor(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_FULL_POWER_BUSY,
			       DM_ISM_EVENT_END_CURSOR_UPDATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_FULL_POWER_RUNNING);
}

/* ===== Tests for dm_ism_next_state — HYSTERESIS_WAITING transitions ===== */

static void dm_test_ism_next_state_hyst_wait_exit_idle(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_HYSTERESIS_WAITING,
			       DM_ISM_EVENT_EXIT_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_TIMER_ABORTED);
}

static void dm_test_ism_next_state_hyst_wait_begin_cursor(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_HYSTERESIS_WAITING,
			       DM_ISM_EVENT_BEGIN_CURSOR_UPDATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_HYSTERESIS_BUSY);
}

static void dm_test_ism_next_state_hyst_wait_timer(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_HYSTERESIS_WAITING,
			       DM_ISM_EVENT_TIMER_ELAPSED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_OPTIMIZED_IDLE);
}

static void dm_test_ism_next_state_hyst_wait_immediate(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_HYSTERESIS_WAITING,
			       DM_ISM_EVENT_IMMEDIATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_OPTIMIZED_IDLE);
}

/* ===== Tests for dm_ism_next_state — HYSTERESIS_BUSY transitions ===== */

static void dm_test_ism_next_state_hyst_busy_exit_idle(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_HYSTERESIS_BUSY,
			       DM_ISM_EVENT_EXIT_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_FULL_POWER_BUSY);
}

static void dm_test_ism_next_state_hyst_busy_end_cursor(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_HYSTERESIS_BUSY,
			       DM_ISM_EVENT_END_CURSOR_UPDATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_HYSTERESIS_WAITING);
}

/* ===== Tests for dm_ism_next_state — OPTIMIZED_IDLE transitions ===== */

static void dm_test_ism_next_state_opt_idle_exit(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_OPTIMIZED_IDLE,
			       DM_ISM_EVENT_EXIT_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_FULL_POWER_RUNNING);
}

static void dm_test_ism_next_state_opt_idle_begin_cursor(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_OPTIMIZED_IDLE,
			       DM_ISM_EVENT_BEGIN_CURSOR_UPDATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_HYSTERESIS_BUSY);
}

static void dm_test_ism_next_state_opt_idle_sso_timer(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_OPTIMIZED_IDLE,
			       DM_ISM_EVENT_SSO_TIMER_ELAPSED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_OPTIMIZED_IDLE_SSO);
}

static void dm_test_ism_next_state_opt_idle_immediate(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_OPTIMIZED_IDLE,
			       DM_ISM_EVENT_IMMEDIATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_OPTIMIZED_IDLE_SSO);
}

/* ===== Tests for dm_ism_next_state — OPTIMIZED_IDLE_SSO transitions ===== */

static void dm_test_ism_next_state_opt_idle_sso_exit(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_OPTIMIZED_IDLE_SSO,
			       DM_ISM_EVENT_EXIT_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_FULL_POWER_RUNNING);
}

static void dm_test_ism_next_state_opt_idle_sso_cursor(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_OPTIMIZED_IDLE_SSO,
			       DM_ISM_EVENT_BEGIN_CURSOR_UPDATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_HYSTERESIS_BUSY);
}

/* ===== Tests for dm_ism_next_state — TIMER_ABORTED transitions ===== */

static void dm_test_ism_next_state_aborted_immediate(struct kunit *test)
{
	enum amdgpu_dm_ism_state next;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_TIMER_ABORTED,
			       DM_ISM_EVENT_IMMEDIATE, &next);
	KUNIT_EXPECT_TRUE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_STATE_FULL_POWER_RUNNING);
}

static void dm_test_ism_next_state_aborted_invalid(struct kunit *test)
{
	enum amdgpu_dm_ism_state next = DM_ISM_NUM_STATES;
	bool ok;

	ok = dm_ism_next_state(DM_ISM_STATE_TIMER_ABORTED,
			       DM_ISM_EVENT_ENTER_IDLE_REQUESTED, &next);
	KUNIT_EXPECT_FALSE(test, ok);
	KUNIT_EXPECT_EQ(test, (int)next, (int)DM_ISM_NUM_STATES);
}

/* ===== Tests for dm_ism_get_sso_delay ===== */

static void dm_test_ism_sso_delay_null_stream(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);

	ism->config.sso_num_frames = 5;

	KUNIT_EXPECT_EQ(test, dm_ism_get_sso_delay(ism, NULL), (uint64_t)0);
}

static void dm_test_ism_sso_delay_zero_frames(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);

	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;
	ism->config.sso_num_frames = 0;

	KUNIT_EXPECT_EQ(test, dm_ism_get_sso_delay(ism, stream), (uint64_t)0);
}

static void dm_test_ism_sso_delay_1080p60_3frames(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);
	uint64_t expected_one_frame_ns, expected;

	/*
	 * 1080p@60Hz: v_total=1125, h_total=2200, pix_clk=148.5MHz
	 * pix_clk_100hz = 1485000
	 * one_frame_ns = (1125 * 2200 * 10000000) / 1485000 = 16666666 ns
	 */
	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;
	ism->config.sso_num_frames = 3;

	expected_one_frame_ns = div64_u64((uint64_t)1125 * 2200 * 10000000ULL,
					  1485000);
	expected = 3 * expected_one_frame_ns;

	KUNIT_EXPECT_EQ(test, dm_ism_get_sso_delay(ism, stream), expected);
}

static void dm_test_ism_sso_delay_4k60_1frame(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);
	uint64_t expected_one_frame_ns;

	/*
	 * 4K@60Hz: v_total=2250, h_total=4400, pix_clk=594MHz
	 * pix_clk_100hz = 5940000
	 */
	stream->timing.v_total = 2250;
	stream->timing.h_total = 4400;
	stream->timing.pix_clk_100hz = 5940000;
	ism->config.sso_num_frames = 1;

	expected_one_frame_ns = div64_u64((uint64_t)2250 * 4400 * 10000000ULL,
					  5940000);

	KUNIT_EXPECT_EQ(test, dm_ism_get_sso_delay(ism, stream),
			expected_one_frame_ns);
}

/* ===== Tests for dm_ism_get_idle_allow_delay ===== */

static void dm_test_ism_idle_delay_null_stream(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);

	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 3;
	ism->config.activation_num_delay_frames = 10;

	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, NULL),
			(uint64_t)0);
}

static void dm_test_ism_idle_delay_zero_filter_frames(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);

	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;
	ism->config.filter_num_frames = 0;

	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			(uint64_t)0);
}

static void dm_test_ism_idle_delay_zero_entry_count(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);

	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;
	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 0;

	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			(uint64_t)0);
}

static void dm_test_ism_idle_delay_zero_delay_frames(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);

	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;
	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 3;
	ism->config.activation_num_delay_frames = 0;

	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			(uint64_t)0);
}

static void dm_test_ism_idle_delay_no_short_idles(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);
	uint64_t one_frame_ns;

	/*
	 * All history records have long durations (well above the
	 * short_idle_ns threshold), so no delay should be applied.
	 */
	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;

	one_frame_ns = div64_u64((uint64_t)1125 * 2200 * 10000000ULL,
				 1485000);

	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 3;
	ism->config.activation_num_delay_frames = 10;
	ism->config.filter_history_size = 8;
	ism->config.filter_old_history_threshold = 0;

	/* Fill history with long idle durations */
	for (int i = 0; i < 8; i++) {
		ism->records[i].duration_ns = one_frame_ns * 100;
		ism->records[i].timestamp_ns = 0;
	}
	ism->next_record_idx = 8;

	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			(uint64_t)0);
}

static void dm_test_ism_idle_delay_enough_short_idles(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);
	uint64_t one_frame_ns, expected;

	/*
	 * Fill history with short idle durations that meet the threshold.
	 * filter_entry_count=3, so 3 short idles should trigger the delay.
	 */
	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;

	one_frame_ns = div64_u64((uint64_t)1125 * 2200 * 10000000ULL,
				 1485000);

	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 3;
	ism->config.activation_num_delay_frames = 10;
	ism->config.filter_history_size = 8;
	ism->config.filter_old_history_threshold = 0;

	/* Fill history with short idle durations (1 frame each) */
	for (int i = 0; i < 8; i++) {
		ism->records[i].duration_ns = one_frame_ns;
		ism->records[i].timestamp_ns = 0;
	}
	ism->next_record_idx = 8;

	expected = 10 * one_frame_ns;
	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			expected);
}

static void dm_test_ism_idle_delay_wraps_around_buffer(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);
	uint64_t one_frame_ns, expected;

	/*
	 * Test the circular buffer wraparound: next_record_idx at 2 means
	 * the most recent records are at indices 1, 0, 15, 14, ...
	 */
	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;

	one_frame_ns = div64_u64((uint64_t)1125 * 2200 * 10000000ULL,
				 1485000);

	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 3;
	ism->config.activation_num_delay_frames = 10;
	ism->config.filter_history_size = 8;
	ism->config.filter_old_history_threshold = 0;

	/* Fill entire buffer with short idles */
	for (int i = 0; i < AMDGPU_DM_IDLE_HIST_LEN; i++) {
		ism->records[i].duration_ns = one_frame_ns;
		ism->records[i].timestamp_ns = 0;
	}
	/* Position next_record_idx at 2 to test wraparound */
	ism->next_record_idx = 2;

	expected = 10 * one_frame_ns;
	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			expected);
}

static void dm_test_ism_idle_delay_old_history_cutoff(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);
	uint64_t one_frame_ns;

	/*
	 * Test old_history_threshold: only recent entries within the
	 * threshold should be counted. Set up 2 recent short idles but
	 * require 3 — older entries are outside the threshold.
	 */
	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;

	one_frame_ns = div64_u64((uint64_t)1125 * 2200 * 10000000ULL,
				 1485000);

	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 3;
	ism->config.activation_num_delay_frames = 10;
	ism->config.filter_history_size = 8;
	/* Threshold: entries older than 20 frames are ignored */
	ism->config.filter_old_history_threshold = 20;

	ism->last_idle_timestamp_ns = one_frame_ns * 100;

	/* 2 recent short idles (within threshold) */
	ism->records[6].duration_ns = one_frame_ns;
	ism->records[6].timestamp_ns = one_frame_ns * 95;
	ism->records[7].duration_ns = one_frame_ns;
	ism->records[7].timestamp_ns = one_frame_ns * 98;

	/* Older entries outside the threshold with long durations */
	for (int i = 0; i < 6; i++) {
		ism->records[i].duration_ns = one_frame_ns * 100;
		ism->records[i].timestamp_ns = one_frame_ns * 10;
	}
	ism->next_record_idx = 8;

	/*
	 * Only 2 short idles within threshold, but 3 required —
	 * should return 0 (no delay).
	 */
	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			(uint64_t)0);
}

static void dm_test_ism_idle_delay_mixed_durations(struct kunit *test)
{
	struct amdgpu_dm_ism *ism = alloc_test_ism(test);
	struct dc_stream_state *stream = alloc_test_stream(test);
	uint64_t one_frame_ns;

	/*
	 * Mix of short and long idle durations. Only 2 short idles
	 * in 8 entries, but filter_entry_count=3, so no delay.
	 */
	stream->timing.v_total = 1125;
	stream->timing.h_total = 2200;
	stream->timing.pix_clk_100hz = 1485000;

	one_frame_ns = div64_u64((uint64_t)1125 * 2200 * 10000000ULL,
				 1485000);

	ism->config.filter_num_frames = 5;
	ism->config.filter_entry_count = 3;
	ism->config.activation_num_delay_frames = 10;
	ism->config.filter_history_size = 8;
	ism->config.filter_old_history_threshold = 0;

	/* 2 short idles, 6 long idles */
	for (int i = 0; i < 8; i++) {
		if (i == 6 || i == 7)
			ism->records[i].duration_ns = one_frame_ns;
		else
			ism->records[i].duration_ns = one_frame_ns * 100;
		ism->records[i].timestamp_ns = 0;
	}
	ism->next_record_idx = 8;

	KUNIT_EXPECT_EQ(test, dm_ism_get_idle_allow_delay(ism, stream),
			(uint64_t)0);
}

static struct kunit_case dm_ism_test_cases[] = {
	/* dm_ism_next_state — FULL_POWER_RUNNING */
	KUNIT_CASE(dm_test_ism_next_state_running_enter_idle),
	KUNIT_CASE(dm_test_ism_next_state_running_begin_cursor),
	KUNIT_CASE(dm_test_ism_next_state_running_invalid),
	/* dm_ism_next_state — FULL_POWER_BUSY */
	KUNIT_CASE(dm_test_ism_next_state_busy_enter_idle),
	KUNIT_CASE(dm_test_ism_next_state_busy_end_cursor),
	/* dm_ism_next_state — HYSTERESIS_WAITING */
	KUNIT_CASE(dm_test_ism_next_state_hyst_wait_exit_idle),
	KUNIT_CASE(dm_test_ism_next_state_hyst_wait_begin_cursor),
	KUNIT_CASE(dm_test_ism_next_state_hyst_wait_timer),
	KUNIT_CASE(dm_test_ism_next_state_hyst_wait_immediate),
	/* dm_ism_next_state — HYSTERESIS_BUSY */
	KUNIT_CASE(dm_test_ism_next_state_hyst_busy_exit_idle),
	KUNIT_CASE(dm_test_ism_next_state_hyst_busy_end_cursor),
	/* dm_ism_next_state — OPTIMIZED_IDLE */
	KUNIT_CASE(dm_test_ism_next_state_opt_idle_exit),
	KUNIT_CASE(dm_test_ism_next_state_opt_idle_begin_cursor),
	KUNIT_CASE(dm_test_ism_next_state_opt_idle_sso_timer),
	KUNIT_CASE(dm_test_ism_next_state_opt_idle_immediate),
	/* dm_ism_next_state — OPTIMIZED_IDLE_SSO */
	KUNIT_CASE(dm_test_ism_next_state_opt_idle_sso_exit),
	KUNIT_CASE(dm_test_ism_next_state_opt_idle_sso_cursor),
	/* dm_ism_next_state — TIMER_ABORTED */
	KUNIT_CASE(dm_test_ism_next_state_aborted_immediate),
	KUNIT_CASE(dm_test_ism_next_state_aborted_invalid),
	/* dm_ism_get_sso_delay */
	KUNIT_CASE(dm_test_ism_sso_delay_null_stream),
	KUNIT_CASE(dm_test_ism_sso_delay_zero_frames),
	KUNIT_CASE(dm_test_ism_sso_delay_1080p60_3frames),
	KUNIT_CASE(dm_test_ism_sso_delay_4k60_1frame),
	/* dm_ism_get_idle_allow_delay */
	KUNIT_CASE(dm_test_ism_idle_delay_null_stream),
	KUNIT_CASE(dm_test_ism_idle_delay_zero_filter_frames),
	KUNIT_CASE(dm_test_ism_idle_delay_zero_entry_count),
	KUNIT_CASE(dm_test_ism_idle_delay_zero_delay_frames),
	KUNIT_CASE(dm_test_ism_idle_delay_no_short_idles),
	KUNIT_CASE(dm_test_ism_idle_delay_enough_short_idles),
	KUNIT_CASE(dm_test_ism_idle_delay_wraps_around_buffer),
	KUNIT_CASE(dm_test_ism_idle_delay_old_history_cutoff),
	KUNIT_CASE(dm_test_ism_idle_delay_mixed_durations),
	{}
};

static struct kunit_suite dm_ism_test_suite = {
	.name = "amdgpu_dm_ism",
	.test_cases = dm_ism_test_cases,
};

kunit_test_suite(dm_ism_test_suite);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_ism");
MODULE_AUTHOR("AMD");
