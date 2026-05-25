// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_mst_types.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>

#include "dc.h"
#include "dpcd_defs.h"
#include "amdgpu_dm_mst_types.h"

/* Tests for needs_dsc_aux_workaround */

/**
 * dm_mst_test_needs_dsc_aux_workaround_match - Test workaround triggers for matching device
 * @test: KUnit test context
 *
 * Verify that needs_dsc_aux_workaround() returns true when the link has
 * the specific branch device ID, DPCD rev 1.4, and sink count >= 2.
 */
static void dm_mst_test_needs_dsc_aux_workaround_match(struct kunit *test)
{
	struct dc_link link = {0};

	link.dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_90CC24;
	link.dpcd_caps.dpcd_rev.raw = DPCD_REV_14;
	link.dpcd_caps.sink_count.bits.SINK_COUNT = 2;

	KUNIT_EXPECT_TRUE(test, needs_dsc_aux_workaround(&link));
}

/**
 * dm_mst_test_needs_dsc_aux_workaround_rev12 - Test workaround triggers for DPCD rev 1.2
 * @test: KUnit test context
 *
 * Verify that needs_dsc_aux_workaround() returns true when the link has
 * the specific branch device ID, DPCD rev 1.2, and sink count >= 2.
 */
static void dm_mst_test_needs_dsc_aux_workaround_rev12(struct kunit *test)
{
	struct dc_link link = {0};

	link.dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_90CC24;
	link.dpcd_caps.dpcd_rev.raw = DPCD_REV_12;
	link.dpcd_caps.sink_count.bits.SINK_COUNT = 3;

	KUNIT_EXPECT_TRUE(test, needs_dsc_aux_workaround(&link));
}

/**
 * dm_mst_test_needs_dsc_aux_workaround_wrong_dev_id - Test workaround skipped for wrong device
 * @test: KUnit test context
 *
 * Verify that needs_dsc_aux_workaround() returns false when the branch
 * device ID does not match DP_BRANCH_DEVICE_ID_90CC24.
 */
static void dm_mst_test_needs_dsc_aux_workaround_wrong_dev_id(struct kunit *test)
{
	struct dc_link link = {0};

	link.dpcd_caps.branch_dev_id = 0x123456;
	link.dpcd_caps.dpcd_rev.raw = DPCD_REV_14;
	link.dpcd_caps.sink_count.bits.SINK_COUNT = 2;

	KUNIT_EXPECT_FALSE(test, needs_dsc_aux_workaround(&link));
}

/**
 * dm_mst_test_needs_dsc_aux_workaround_wrong_rev - Test workaround skipped for unsupported rev
 * @test: KUnit test context
 *
 * Verify that needs_dsc_aux_workaround() returns false when the DPCD
 * revision is neither 1.2 nor 1.4.
 */
static void dm_mst_test_needs_dsc_aux_workaround_wrong_rev(struct kunit *test)
{
	struct dc_link link = {0};

	link.dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_90CC24;
	link.dpcd_caps.dpcd_rev.raw = 0x11; /* DPCD 1.1 */
	link.dpcd_caps.sink_count.bits.SINK_COUNT = 2;

	KUNIT_EXPECT_FALSE(test, needs_dsc_aux_workaround(&link));
}

/**
 * dm_mst_test_needs_dsc_aux_workaround_low_sink_count - Test workaround skipped for single sink
 * @test: KUnit test context
 *
 * Verify that needs_dsc_aux_workaround() returns false when the sink
 * count is less than 2, even if device ID and DPCD rev match.
 */
static void dm_mst_test_needs_dsc_aux_workaround_low_sink_count(struct kunit *test)
{
	struct dc_link link = {0};

	link.dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_90CC24;
	link.dpcd_caps.dpcd_rev.raw = DPCD_REV_14;
	link.dpcd_caps.sink_count.bits.SINK_COUNT = 1;

	KUNIT_EXPECT_FALSE(test, needs_dsc_aux_workaround(&link));
}

static struct kunit_case dm_mst_types_test_cases[] = {
	/* needs_dsc_aux_workaround tests */
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_match),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_rev12),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_wrong_dev_id),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_wrong_rev),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_low_sink_count),
	{}
};

static struct kunit_suite dm_mst_types_test_suite = {
	.name = "amdgpu_dm_mst_types",
	.test_cases = dm_mst_types_test_cases,
};

kunit_test_suite(dm_mst_types_test_suite);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_mst_types");
