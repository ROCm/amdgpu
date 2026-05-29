// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_pp_smu.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <linux/types.h>

#include "dc.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_pp_smu.h"

/* ---- Tests for get_default_clock_levels ---- */

/**
 * dm_test_default_clock_levels_display - Test display clock default levels
 * @test: KUnit test context
 *
 * Verify that get_default_clock_levels populates 6 display clock levels
 * with the expected frequencies in kHz.
 */
static void dm_test_default_clock_levels_display(struct kunit *test)
{
	struct dm_pp_clock_levels clks = { 0 };
	uint32_t expected[] = { 300000, 400000, 496560, 626090, 685720, 757900 };
	int i;

	get_default_clock_levels(DM_PP_CLOCK_TYPE_DISPLAY_CLK, &clks);

	KUNIT_EXPECT_EQ(test, clks.num_levels, 6U);
	for (i = 0; i < 6; i++)
		KUNIT_EXPECT_EQ(test, clks.clocks_in_khz[i], expected[i]);
}

/**
 * dm_test_default_clock_levels_engine - Test engine clock default levels
 * @test: KUnit test context
 *
 * Verify that get_default_clock_levels populates 6 engine clock levels
 * with the expected frequencies in kHz.
 */
static void dm_test_default_clock_levels_engine(struct kunit *test)
{
	struct dm_pp_clock_levels clks = { 0 };
	uint32_t expected[] = { 300000, 360000, 423530, 514290, 626090, 720000 };
	int i;

	get_default_clock_levels(DM_PP_CLOCK_TYPE_ENGINE_CLK, &clks);

	KUNIT_EXPECT_EQ(test, clks.num_levels, 6U);
	for (i = 0; i < 6; i++)
		KUNIT_EXPECT_EQ(test, clks.clocks_in_khz[i], expected[i]);
}

/**
 * dm_test_default_clock_levels_memory - Test memory clock default levels
 * @test: KUnit test context
 *
 * Verify that get_default_clock_levels populates 2 memory clock levels
 * with the expected frequencies in kHz.
 */
static void dm_test_default_clock_levels_memory(struct kunit *test)
{
	struct dm_pp_clock_levels clks = { 0 };

	get_default_clock_levels(DM_PP_CLOCK_TYPE_MEMORY_CLK, &clks);

	KUNIT_EXPECT_EQ(test, clks.num_levels, 2U);
	KUNIT_EXPECT_EQ(test, clks.clocks_in_khz[0], 333000U);
	KUNIT_EXPECT_EQ(test, clks.clocks_in_khz[1], 800000U);
}

/**
 * dm_test_default_clock_levels_unknown - Test unknown clock type default
 * @test: KUnit test context
 *
 * Verify that get_default_clock_levels sets num_levels to 0 for an
 * unrecognized clock type.
 */
static void dm_test_default_clock_levels_unknown(struct kunit *test)
{
	struct dm_pp_clock_levels clks = { 0 };

	get_default_clock_levels(DM_PP_CLOCK_TYPE_FCLK, &clks);

	KUNIT_EXPECT_EQ(test, clks.num_levels, 0U);
}

/* ---- Tests for dc_to_pp_clock_type ---- */

/**
 * dm_test_dc_to_pp_clock_type_display - Test display clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_DISPLAY_CLK maps to amd_pp_disp_clock.
 */
static void dm_test_dc_to_pp_clock_type_display(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_DISPLAY_CLK),
			(int)amd_pp_disp_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_engine - Test engine clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_ENGINE_CLK maps to amd_pp_sys_clock.
 */
static void dm_test_dc_to_pp_clock_type_engine(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_ENGINE_CLK),
			(int)amd_pp_sys_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_memory - Test memory clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_MEMORY_CLK maps to amd_pp_mem_clock.
 */
static void dm_test_dc_to_pp_clock_type_memory(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_MEMORY_CLK),
			(int)amd_pp_mem_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_dcefclk - Test DCEF clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_DCEFCLK maps to amd_pp_dcef_clock.
 */
static void dm_test_dc_to_pp_clock_type_dcefclk(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_DCEFCLK),
			(int)amd_pp_dcef_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_dcfclk - Test DCF clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_DCFCLK maps to amd_pp_dcf_clock.
 */
static void dm_test_dc_to_pp_clock_type_dcfclk(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_DCFCLK),
			(int)amd_pp_dcf_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_pixelclk - Test pixel clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_PIXELCLK maps to amd_pp_pixel_clock.
 */
static void dm_test_dc_to_pp_clock_type_pixelclk(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_PIXELCLK),
			(int)amd_pp_pixel_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_fclk - Test FCLK type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_FCLK maps to amd_pp_f_clock.
 */
static void dm_test_dc_to_pp_clock_type_fclk(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_FCLK),
			(int)amd_pp_f_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_phyclk - Test display PHY clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_DISPLAYPHYCLK maps to amd_pp_phy_clock.
 */
static void dm_test_dc_to_pp_clock_type_phyclk(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_DISPLAYPHYCLK),
			(int)amd_pp_phy_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_dppclk - Test DPP clock type mapping
 * @test: KUnit test context
 *
 * Verify DM_PP_CLOCK_TYPE_DPPCLK maps to amd_pp_dpp_clock.
 */
static void dm_test_dc_to_pp_clock_type_dppclk(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(DM_PP_CLOCK_TYPE_DPPCLK),
			(int)amd_pp_dpp_clock);
}

/**
 * dm_test_dc_to_pp_clock_type_invalid - Test invalid clock type mapping
 * @test: KUnit test context
 *
 * Verify that an invalid clock type value maps to 0.
 */
static void dm_test_dc_to_pp_clock_type_invalid(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)dc_to_pp_clock_type(0), 0);
}

static struct kunit_case dm_pp_smu_test_cases[] = {
	/* get_default_clock_levels */
	KUNIT_CASE(dm_test_default_clock_levels_display),
	KUNIT_CASE(dm_test_default_clock_levels_engine),
	KUNIT_CASE(dm_test_default_clock_levels_memory),
	KUNIT_CASE(dm_test_default_clock_levels_unknown),
	/* dc_to_pp_clock_type */
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_display),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_engine),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_memory),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_dcefclk),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_dcfclk),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_pixelclk),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_fclk),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_phyclk),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_dppclk),
	KUNIT_CASE(dm_test_dc_to_pp_clock_type_invalid),
	{}
};

static struct kunit_suite dm_pp_smu_test_suite = {
	.name = "amdgpu_dm_pp_smu",
	.test_cases = dm_pp_smu_test_cases,
};

kunit_test_suite(dm_pp_smu_test_suite);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_pp_smu");
