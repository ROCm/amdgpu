// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_backlight.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>

#include "dc.h"
#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_backlight.h"
#include "amd_shared.h"

struct dm_backlight_connector_fixture {
	struct amdgpu_device *adev;
	struct amdgpu_dm_connector *aconnector;
	struct dc_link *link;
};

static struct amdgpu_display_manager *alloc_test_dm(struct kunit *test)
{
	struct amdgpu_display_manager *dm;

	dm = kunit_kzalloc(test, sizeof(*dm), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dm);

	return dm;
}

static void setup_test_connector(struct kunit *test,
				 struct dm_backlight_connector_fixture *fixture,
				 int bl_idx, enum signal_type signal)
{
	fixture->adev = kunit_kzalloc(test, sizeof(*fixture->adev), GFP_KERNEL);
	fixture->aconnector = kunit_kzalloc(test, sizeof(*fixture->aconnector), GFP_KERNEL);
	fixture->link = kunit_kzalloc(test, sizeof(*fixture->link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->aconnector);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->link);

	fixture->aconnector->bl_idx = bl_idx;
	fixture->aconnector->dc_link = fixture->link;
	fixture->aconnector->base.dev = &fixture->adev->ddev;
	fixture->link->connector_signal = signal;
}

/* Tests for amdgpu_dm_update_backlight_caps() */

/**
 * dm_test_backlight_caps_valid_short_circuit - Test Backlight caps valid short circuit
 * @test: The KUnit test context
 */
static void dm_test_backlight_caps_valid_short_circuit(struct kunit *test)
{
	struct amdgpu_display_manager *dm = alloc_test_dm(test);
	struct amdgpu_dm_backlight_caps *caps = &dm->backlight_caps[0];

	caps->caps_valid = true;
	caps->aux_support = false;
	caps->min_input_signal = 42;
	caps->max_input_signal = 199;

	amdgpu_dm_update_backlight_caps(dm, 0);

	KUNIT_EXPECT_TRUE(test, caps->caps_valid);
	KUNIT_EXPECT_EQ(test, caps->min_input_signal, 42);
	KUNIT_EXPECT_EQ(test, caps->max_input_signal, 199);
}

#if !defined(CONFIG_ACPI)

/**
 * dm_test_backlight_caps_aux_support_noop - Test Backlight caps aux support noop
 * @test: The KUnit test context
 */
static void dm_test_backlight_caps_aux_support_noop(struct kunit *test)
{
	struct amdgpu_display_manager *dm = alloc_test_dm(test);
	struct amdgpu_dm_backlight_caps *caps = &dm->backlight_caps[0];

	caps->caps_valid = false;
	caps->aux_support = true;
	caps->min_input_signal = 11;
	caps->max_input_signal = 222;

	amdgpu_dm_update_backlight_caps(dm, 0);

	KUNIT_EXPECT_FALSE(test, caps->caps_valid);
	KUNIT_EXPECT_EQ(test, caps->min_input_signal, 11);
	KUNIT_EXPECT_EQ(test, caps->max_input_signal, 222);
}

/**
 * dm_test_backlight_caps_non_aux_sets_defaults - Test Backlight caps non aux sets defaults
 * @test: The KUnit test context
 */
static void dm_test_backlight_caps_non_aux_sets_defaults(struct kunit *test)
{
	struct amdgpu_display_manager *dm = alloc_test_dm(test);
	struct amdgpu_dm_backlight_caps *caps = &dm->backlight_caps[0];

	caps->caps_valid = false;
	caps->aux_support = false;
	caps->min_input_signal = 0;
	caps->max_input_signal = 0;

	amdgpu_dm_update_backlight_caps(dm, 0);

	KUNIT_EXPECT_TRUE(test, caps->caps_valid);
	KUNIT_EXPECT_EQ(test, caps->min_input_signal, AMDGPU_DM_DEFAULT_MIN_BACKLIGHT);
	KUNIT_EXPECT_EQ(test, caps->max_input_signal, AMDGPU_DM_DEFAULT_MAX_BACKLIGHT);
}
#endif

/* Tests for get_brightness_range() */

/**
 * dm_test_brightness_range_null_caps - Test Brightness range null caps
 * @test: The KUnit test context
 */
static void dm_test_brightness_range_null_caps(struct kunit *test)
{
	unsigned int min = 99, max = 99;

	KUNIT_EXPECT_EQ(test, get_brightness_range(NULL, &min, &max), 0);
	/* min/max should remain untouched */
	KUNIT_EXPECT_EQ(test, min, 99U);
	KUNIT_EXPECT_EQ(test, max, 99U);
}

/**
 * dm_test_brightness_range_pwm - Test Brightness range pwm
 * @test: The KUnit test context
 */
static void dm_test_brightness_range_pwm(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;

	KUNIT_EXPECT_EQ(test, get_brightness_range(&caps, &min, &max), 1);
	/* 0x101 * AMDGPU_DM_DEFAULT_MIN_BACKLIGHT, 0x101 * AMDGPU_DM_DEFAULT_MAX_BACKLIGHT */
	KUNIT_EXPECT_EQ(test, min, 0x101U * AMDGPU_DM_DEFAULT_MIN_BACKLIGHT);
	KUNIT_EXPECT_EQ(test, max, 0x101U * AMDGPU_DM_DEFAULT_MAX_BACKLIGHT);
}

/**
 * dm_test_brightness_range_aux - Test Brightness range aux
 * @test: The KUnit test context
 */
static void dm_test_brightness_range_aux(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;

	caps.aux_support = true;
	caps.aux_min_input_signal = 1;
	caps.aux_max_input_signal = 512;

	KUNIT_EXPECT_EQ(test, get_brightness_range(&caps, &min, &max), 1);
	/* millinits: 1000 * value */
	KUNIT_EXPECT_EQ(test, min, 1000U);
	KUNIT_EXPECT_EQ(test, max, 512000U);
}

/* Tests for convert_brightness_to_user() */

/**
 * dm_test_brightness_to_user_null_caps - Test Brightness to user null caps
 * @test: The KUnit test context
 */
static void dm_test_brightness_to_user_null_caps(struct kunit *test)
{
	/*
	 * With NULL caps, get_brightness_range fails → passthrough.
	 * We simulate this by passing a zeroed caps struct where
	 * max_input_signal=0 makes max=0 and the function hits
	 * get_brightness_range returning 0 since caps is NULL.
	 */
	KUNIT_EXPECT_EQ(test, convert_brightness_to_user(NULL, 42), 42U);
}

/**
 * dm_test_brightness_to_user_below_min - Test Brightness to user below min
 * @test: The KUnit test context
 */
static void dm_test_brightness_to_user_below_min(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;

	/* brightness < min (0x101*AMDGPU_DM_DEFAULT_MIN_BACKLIGHT), should return 0 */
	KUNIT_EXPECT_EQ(test, convert_brightness_to_user(&caps, 100), 0U);
}

/**
 * dm_test_brightness_to_user_at_max - Test Brightness to user at max
 * @test: The KUnit test context
 */
static void dm_test_brightness_to_user_at_max(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;

	get_brightness_range(&caps, &min, &max);

	/* At max → should return max */
	KUNIT_EXPECT_EQ(test, convert_brightness_to_user(&caps, max), max);
}

/**
 * dm_test_brightness_to_user_at_min - Test Brightness to user at min
 * @test: The KUnit test context
 */
static void dm_test_brightness_to_user_at_min(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;

	get_brightness_range(&caps, &min, &max);

	/* At min → should return 0 */
	KUNIT_EXPECT_EQ(test, convert_brightness_to_user(&caps, min), 0U);
}

/**
 * dm_test_brightness_to_user_midpoint_pwm - Test Brightness to user midpoint pwm
 * @test: The KUnit test context
 */
static void dm_test_brightness_to_user_midpoint_pwm(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max, mid_hw, result;
	u64 expected;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;

	get_brightness_range(&caps, &min, &max);

	/* midpoint of hw range */
	mid_hw = min + (max - min) / 2;
	/* expected = DIV_ROUND_CLOSEST_ULL((u64)max * (mid_hw - min), max - min) */
	expected = DIV_ROUND_CLOSEST_ULL((u64)max * (mid_hw - min), max - min);
	result = convert_brightness_to_user(&caps, mid_hw);

	KUNIT_EXPECT_EQ(test, result, (u32)expected);
}

/* Tests for convert_brightness_from_user() — no custom curve */

/**
 * dm_test_brightness_from_user_null_caps - Test Brightness from user null caps
 * @test: The KUnit test context
 */
static void dm_test_brightness_from_user_null_caps(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, convert_brightness_from_user(NULL, 100), 100U);
}

/**
 * dm_test_brightness_from_user_zero - Test Brightness from user zero
 * @test: The KUnit test context
 */
static void dm_test_brightness_from_user_zero(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	/* no custom curve */
	caps.data_points = 0;

	get_brightness_range(&caps, &min, &max);

	/* brightness=0 → min + 0 = min */
	KUNIT_EXPECT_EQ(test, convert_brightness_from_user(&caps, 0), (u32)min);
}

/**
 * dm_test_brightness_from_user_max - Test Brightness from user max
 * @test: The KUnit test context
 */
static void dm_test_brightness_from_user_max(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 0;

	get_brightness_range(&caps, &min, &max);

	/*
	 * brightness=max → min + DIV_ROUND_CLOSEST((max-min)*max, max)
	 *               = min + (max - min) = max
	 */
	KUNIT_EXPECT_EQ(test, convert_brightness_from_user(&caps, max), (u32)max);
}

/**
 * dm_test_brightness_from_user_aux - Test Brightness from user aux
 * @test: The KUnit test context
 */
static void dm_test_brightness_from_user_aux(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;

	caps.aux_support = true;
	caps.aux_min_input_signal = 1;
	caps.aux_max_input_signal = 512;
	caps.data_points = 0;

	get_brightness_range(&caps, &min, &max);

	/* brightness=0 → min */
	KUNIT_EXPECT_EQ(test, convert_brightness_from_user(&caps, 0), (u32)min);
	/* brightness=max → max */
	KUNIT_EXPECT_EQ(test, convert_brightness_from_user(&caps, max), (u32)max);
}

/* Tests for convert_custom_brightness() */

/**
 * dm_test_custom_brightness_no_data_points - Test Custom brightness no data points
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_no_data_points(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness = 128;
	uint32_t saved = brightness;

	caps.data_points = 0;

	convert_custom_brightness(&caps, 3084, 65535, &brightness);

	/* No data points → no-op */
	KUNIT_EXPECT_EQ(test, brightness, saved);
}

/**
 * dm_test_custom_brightness_debug_mask_disables - Test Custom brightness debug mask disables
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_debug_mask_disables(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness = 128;
	uint32_t saved = brightness;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	caps.data_points = 3;
	caps.luminance_data[0].input_signal = 50;
	caps.luminance_data[0].luminance = 10;

	/* Set the disable flag */
	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() | DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	convert_custom_brightness(&caps, 3084, 65535, &brightness);

	/* Should be no-op due to debug mask */
	KUNIT_EXPECT_EQ(test, brightness, saved);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_custom_brightness_exact_match - Test Custom brightness exact match
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_exact_match(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness;
	unsigned int min, max;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() & ~DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 3;
	caps.luminance_data[0].input_signal = 50;
	caps.luminance_data[0].luminance = 20;
	caps.luminance_data[1].input_signal = 128;
	caps.luminance_data[1].luminance = 50;
	caps.luminance_data[2].input_signal = 200;
	caps.luminance_data[2].luminance = 90;

	get_brightness_range(&caps, &min, &max);

	/*
	 * Set brightness so that scale_input_to_fw yields exactly 128.
	 * scale_input_to_fw(min, max, x) = DIV_ROUND_CLOSEST(x * 255, max - min)
	 * With min=0, max=0x101*255=65535:
	 * We need x such that DIV_ROUND_CLOSEST(x * 255, 65535) = 128
	 * → x = 128 * 65535 / 255 = 32896
	 */
	brightness = 32896;

	convert_custom_brightness(&caps, min, max, &brightness);

	/*
	 * Exact match: lum=50, brightness_scaled=128
	 * result = scale_fw_to_input(min, max, DIV_ROUND_CLOSEST(50*128, 101))
	 *        = scale_fw_to_input(0, 65535, DIV_ROUND_CLOSEST(6400, 101))
	 *        = scale_fw_to_input(0, 65535, 63)
	 *        = 0 + DIV_ROUND_CLOSEST(63 * 65535, 255) = 16191 (approx)
	 */
	KUNIT_EXPECT_TRUE(test, brightness != 32896);
	KUNIT_EXPECT_TRUE(test, brightness < 32896);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_custom_brightness_below_first - Test Custom brightness below first
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_below_first(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness;
	unsigned int min, max;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() & ~DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 2;
	caps.luminance_data[0].input_signal = 100;
	caps.luminance_data[0].luminance = 40;
	caps.luminance_data[1].input_signal = 200;
	caps.luminance_data[1].luminance = 80;

	get_brightness_range(&caps, &min, &max);

	/*
	 * Set brightness low enough that scaled value < 100.
	 * scale_input_to_fw(0, 65535, x) = DIV_ROUND_CLOSEST(x*255, 65535)
	 * For result=50: x = 50*65535/255 = 12850
	 */
	brightness = 12850;

	convert_custom_brightness(&caps, min, max, &brightness);

	/*
	 * Below first data point: lum = DIV_ROUND_CLOSEST(40 * 50, 100) = 20
	 * Then: scale_fw_to_input(0, 65535, DIV_ROUND_CLOSEST(20 * 50, 101))
	 *      = scale_fw_to_input(0, 65535, DIV_ROUND_CLOSEST(1000, 101))
	 *      = scale_fw_to_input(0, 65535, 10)
	 * The output should be significantly less than input.
	 */
	KUNIT_EXPECT_TRUE(test, brightness < 12850);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_custom_brightness_interpolation - Test Custom brightness interpolation
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_interpolation(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness;
	unsigned int min, max;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() & ~DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 2;
	caps.luminance_data[0].input_signal = 50;
	caps.luminance_data[0].luminance = 20;
	caps.luminance_data[1].input_signal = 200;
	caps.luminance_data[1].luminance = 80;

	get_brightness_range(&caps, &min, &max);

	/*
	 * Choose a value between data points 50 and 200.
	 * scale_input_to_fw(0, 65535, x) = 125 when x = 125*65535/255 = 32125
	 */
	brightness = 32125;

	convert_custom_brightness(&caps, min, max, &brightness);

	/*
	 * The function should interpolate between data points and produce
	 * a remapped value different from the input.
	 */
	KUNIT_EXPECT_TRUE(test, brightness != 32125);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_custom_brightness_above_last - Test Custom brightness above last data point
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_above_last(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness;
	unsigned int min, max;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() & ~DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 2;
	caps.luminance_data[0].input_signal = 50;
	caps.luminance_data[0].luminance = 20;
	caps.luminance_data[1].input_signal = 150;
	caps.luminance_data[1].luminance = 60;

	get_brightness_range(&caps, &min, &max);

	/*
	 * Choose brightness above the last data point (150).
	 * scale_input_to_fw(0, 65535, x) = 220 when x = 220*65535/255 = 56533
	 * After binary search, left >= data_points, clamped → right==left,
	 * so lum = upper_lum = 60.
	 */
	brightness = 56533;

	convert_custom_brightness(&caps, min, max, &brightness);

	/* Output should differ from input (remapped via curve) */
	KUNIT_EXPECT_TRUE(test, brightness != 56533);
	KUNIT_EXPECT_TRUE(test, brightness < 56533);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_custom_brightness_single_data_point - Test Custom brightness with single data point
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_single_data_point(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness;
	unsigned int min, max;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() & ~DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 1;
	caps.luminance_data[0].input_signal = 128;
	caps.luminance_data[0].luminance = 50;

	get_brightness_range(&caps, &min, &max);

	/*
	 * Brightness below the single data point triggers the
	 * "below first" path: lum = DIV_ROUND_CLOSEST(50 * scaled, 128).
	 * scale_input_to_fw(0, 65535, x) = 64 when x = 64*65535/255 = 16448
	 */
	brightness = 16448;

	convert_custom_brightness(&caps, min, max, &brightness);

	KUNIT_EXPECT_TRUE(test, brightness < 16448);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_custom_brightness_lower_lum_zero - Test Custom brightness with zero lower luminance
 * @test: The KUnit test context
 */
static void dm_test_custom_brightness_lower_lum_zero(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	uint32_t brightness;
	unsigned int min, max;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() & ~DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 2;
	caps.luminance_data[0].input_signal = 50;
	caps.luminance_data[0].luminance = 0;	/* zero lower luminance */
	caps.luminance_data[1].input_signal = 200;
	caps.luminance_data[1].luminance = 80;

	get_brightness_range(&caps, &min, &max);

	/*
	 * Choose brightness between data points to trigger interpolation.
	 * scale_input_to_fw(0, 65535, x) = 125 when x = 125*65535/255 = 32125
	 * With lower_lum == 0, code takes shortcut: lum = upper_lum = 80.
	 */
	brightness = 32125;

	convert_custom_brightness(&caps, min, max, &brightness);

	/* Should remap; result should differ from input */
	KUNIT_EXPECT_TRUE(test, brightness != 32125);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_brightness_to_user_above_max - Test Brightness to user above max
 * @test: The KUnit test context
 */
static void dm_test_brightness_to_user_above_max(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max, result;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;

	get_brightness_range(&caps, &min, &max);

	/* brightness above max → result > max (linear extrapolation) */
	result = convert_brightness_to_user(&caps, max + 1000);

	KUNIT_EXPECT_GT(test, result, max);
}

/**
 * dm_test_brightness_from_user_midrange - Test Brightness from user mid-range value
 * @test: The KUnit test context
 */
static void dm_test_brightness_from_user_midrange(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;
	u32 result;

	caps.aux_support = false;
	caps.min_input_signal = AMDGPU_DM_DEFAULT_MIN_BACKLIGHT;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 0;

	get_brightness_range(&caps, &min, &max);

	/* Mid-range brightness should map to between min and max */
	result = convert_brightness_from_user(&caps, max / 2);

	KUNIT_EXPECT_GE(test, result, min);
	KUNIT_EXPECT_LE(test, result, max);
}

/**
 * dm_test_brightness_from_user_with_curve - Test Brightness from user with custom curve active
 * @test: The KUnit test context
 */
static void dm_test_brightness_from_user_with_curve(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min, max;
	u32 with_curve, without_curve;
	uint saved_mask = amdgpu_dm_get_dc_debug_mask();

	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() & ~DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = AMDGPU_DM_DEFAULT_MAX_BACKLIGHT;
	caps.data_points = 2;
	caps.luminance_data[0].input_signal = 50;
	caps.luminance_data[0].luminance = 20;
	caps.luminance_data[1].input_signal = 200;
	caps.luminance_data[1].luminance = 80;

	get_brightness_range(&caps, &min, &max);

	with_curve = convert_brightness_from_user(&caps, max / 2);

	/* Now disable the curve and compare */
	amdgpu_dm_set_dc_debug_mask(amdgpu_dm_get_dc_debug_mask() | DC_DISABLE_CUSTOM_BRIGHTNESS_CURVE);
	without_curve = convert_brightness_from_user(&caps, max / 2);

	/* Custom curve should produce a different mapping */
	KUNIT_EXPECT_NE(test, with_curve, without_curve);

	amdgpu_dm_set_dc_debug_mask(saved_mask);
}

/**
 * dm_test_brightness_range_zero_signals - Test Brightness range with zero min and max signals
 * @test: The KUnit test context
 */
static void dm_test_brightness_range_zero_signals(struct kunit *test)
{
	struct amdgpu_dm_backlight_caps caps = {};
	unsigned int min = 99, max = 99;

	caps.aux_support = false;
	caps.min_input_signal = 0;
	caps.max_input_signal = 0;

	/* Both signals zero → min=max=0 */
	KUNIT_EXPECT_EQ(test, get_brightness_range(&caps, &min, &max), 1);
	KUNIT_EXPECT_EQ(test, min, 0U);
	KUNIT_EXPECT_EQ(test, max, 0U);
}

/* Tests for amdgpu_dm_update_connector_ext_caps() */

/**
 * dm_test_update_connector_ext_caps_negative_bl_idx - Test negative backlight index early return
 * @test: The KUnit test context
 */
static void dm_test_update_connector_ext_caps_negative_bl_idx(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;

	aconnector = kunit_kzalloc(test, sizeof(*aconnector), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, aconnector);

	aconnector->bl_idx = -1;

	amdgpu_dm_update_connector_ext_caps(aconnector);

	KUNIT_SUCCEED(test);
}

/**
 * dm_test_update_connector_ext_caps_non_edp - Test non-eDP connector early return
 * @test: The KUnit test context
 */
static void dm_test_update_connector_ext_caps_non_edp(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};

	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_HDMI_TYPE_A);
	fixture.adev->dm.backlight_caps[0].aux_support = true;

	amdgpu_dm_update_connector_ext_caps(fixture.aconnector);

	KUNIT_EXPECT_TRUE(test, fixture.adev->dm.backlight_caps[0].aux_support);
	KUNIT_EXPECT_PTR_EQ(test, fixture.adev->dm.backlight_caps[0].ext_caps, NULL);
}

/**
 * dm_test_update_connector_ext_caps_oled_defaults - Test OLED eDP defaults to AUX backlight
 * @test: The KUnit test context
 */
static void dm_test_update_connector_ext_caps_oled_defaults(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_backlight = amdgpu_dm_get_backlight_param();

	amdgpu_dm_set_backlight_param(-1);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_EDP);
	fixture.link->dpcd_sink_ext_caps.bits.oled = 1;

	amdgpu_dm_update_connector_ext_caps(fixture.aconnector);

	KUNIT_EXPECT_PTR_EQ(test, fixture.adev->dm.backlight_caps[0].ext_caps,
			    &fixture.link->dpcd_sink_ext_caps);
	KUNIT_EXPECT_TRUE(test, fixture.adev->dm.backlight_caps[0].aux_support);
	KUNIT_EXPECT_EQ(test, fixture.link->backlight_control_type,
			BACKLIGHT_CONTROL_AMD_AUX);
	KUNIT_EXPECT_EQ(test, fixture.adev->dm.backlight_caps[0].aux_max_input_signal, 512);
	KUNIT_EXPECT_EQ(test, fixture.adev->dm.backlight_caps[0].aux_min_input_signal, 1);

	amdgpu_dm_set_backlight_param(saved_backlight);
}

/**
 * dm_test_update_connector_ext_caps_luminance_values - Test luminance range copy
 * @test: The KUnit test context
 */
static void dm_test_update_connector_ext_caps_luminance_values(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_backlight = amdgpu_dm_get_backlight_param();

	amdgpu_dm_set_backlight_param(-1);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_EDP);
	fixture.aconnector->base.display_info.luminance_range.min_luminance = 2;
	fixture.aconnector->base.display_info.luminance_range.max_luminance = 400;

	amdgpu_dm_update_connector_ext_caps(fixture.aconnector);

	KUNIT_EXPECT_FALSE(test, fixture.adev->dm.backlight_caps[0].aux_support);
	KUNIT_EXPECT_EQ(test, fixture.adev->dm.backlight_caps[0].aux_max_input_signal, 400);
	KUNIT_EXPECT_EQ(test, fixture.adev->dm.backlight_caps[0].aux_min_input_signal, 2);

	amdgpu_dm_set_backlight_param(saved_backlight);
}

/**
 * dm_test_update_connector_ext_caps_force_aux - Test module parameter forces AUX backlight
 * @test: The KUnit test context
 */
static void dm_test_update_connector_ext_caps_force_aux(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_backlight = amdgpu_dm_get_backlight_param();

	amdgpu_dm_set_backlight_param(1);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_EDP);

	amdgpu_dm_update_connector_ext_caps(fixture.aconnector);

	KUNIT_EXPECT_TRUE(test, fixture.adev->dm.backlight_caps[0].aux_support);
	KUNIT_EXPECT_EQ(test, fixture.link->backlight_control_type,
			BACKLIGHT_CONTROL_AMD_AUX);

	amdgpu_dm_set_backlight_param(saved_backlight);
}

/**
 * dm_test_update_connector_ext_caps_force_pwm - Test module parameter forces PWM backlight
 * @test: The KUnit test context
 */
static void dm_test_update_connector_ext_caps_force_pwm(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_backlight = amdgpu_dm_get_backlight_param();

	amdgpu_dm_set_backlight_param(0);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_EDP);
	fixture.link->dpcd_sink_ext_caps.bits.oled = 1;

	amdgpu_dm_update_connector_ext_caps(fixture.aconnector);

	KUNIT_EXPECT_FALSE(test, fixture.adev->dm.backlight_caps[0].aux_support);
	KUNIT_EXPECT_NE(test, fixture.link->backlight_control_type,
			BACKLIGHT_CONTROL_AMD_AUX);

	amdgpu_dm_set_backlight_param(saved_backlight);
}

/* Tests for amdgpu_dm_should_create_sysfs() */

/**
 * dm_test_should_create_sysfs_abm_forced - Test forced ABM disables sysfs
 * @test: The KUnit test context
 */
static void dm_test_should_create_sysfs_abm_forced(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_abm_level = amdgpu_dm_get_abm_level_param();

	amdgpu_dm_set_abm_level_param(1);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_EDP);
	fixture.aconnector->base.connector_type = DRM_MODE_CONNECTOR_eDP;

	KUNIT_EXPECT_FALSE(test, amdgpu_dm_should_create_sysfs(fixture.aconnector));

	amdgpu_dm_set_abm_level_param(saved_abm_level);
}

/**
 * dm_test_should_create_sysfs_non_edp - Test non-eDP connector disables sysfs
 * @test: The KUnit test context
 */
static void dm_test_should_create_sysfs_non_edp(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_abm_level = amdgpu_dm_get_abm_level_param();

	amdgpu_dm_set_abm_level_param(-1);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_HDMI_TYPE_A);
	fixture.aconnector->base.connector_type = DRM_MODE_CONNECTOR_HDMIA;

	KUNIT_EXPECT_FALSE(test, amdgpu_dm_should_create_sysfs(fixture.aconnector));

	amdgpu_dm_set_abm_level_param(saved_abm_level);
}

/**
 * dm_test_should_create_sysfs_no_backlight_index - Test eDP without backlight index enables sysfs
 * @test: The KUnit test context
 */
static void dm_test_should_create_sysfs_no_backlight_index(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_abm_level = amdgpu_dm_get_abm_level_param();

	amdgpu_dm_set_abm_level_param(-1);
	setup_test_connector(test, &fixture, -1, SIGNAL_TYPE_EDP);
	fixture.aconnector->base.connector_type = DRM_MODE_CONNECTOR_eDP;

	KUNIT_EXPECT_TRUE(test, amdgpu_dm_should_create_sysfs(fixture.aconnector));

	amdgpu_dm_set_abm_level_param(saved_abm_level);
}

/**
 * dm_test_should_create_sysfs_aux_backlight - Test AUX backlight disables sysfs
 * @test: The KUnit test context
 */
static void dm_test_should_create_sysfs_aux_backlight(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_abm_level = amdgpu_dm_get_abm_level_param();

	amdgpu_dm_set_abm_level_param(-1);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_EDP);
	fixture.aconnector->base.connector_type = DRM_MODE_CONNECTOR_eDP;
	fixture.adev->dm.backlight_caps[0].aux_support = true;

	KUNIT_EXPECT_FALSE(test, amdgpu_dm_should_create_sysfs(fixture.aconnector));

	amdgpu_dm_set_abm_level_param(saved_abm_level);
}

/**
 * dm_test_should_create_sysfs_pwm_backlight - Test PWM backlight enables sysfs
 * @test: The KUnit test context
 */
static void dm_test_should_create_sysfs_pwm_backlight(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	int saved_abm_level = amdgpu_dm_get_abm_level_param();

	amdgpu_dm_set_abm_level_param(-1);
	setup_test_connector(test, &fixture, 0, SIGNAL_TYPE_EDP);
	fixture.aconnector->base.connector_type = DRM_MODE_CONNECTOR_eDP;
	fixture.adev->dm.backlight_caps[0].aux_support = false;

	KUNIT_EXPECT_TRUE(test, amdgpu_dm_should_create_sysfs(fixture.aconnector));

	amdgpu_dm_set_abm_level_param(saved_abm_level);
}

/* Tests for amdgpu_dm_setup_backlight_device() */

/**
 * dm_test_setup_backlight_device_non_edp - Test non-eDP/LVDS link is skipped
 * @test: The KUnit test context
 */
static void dm_test_setup_backlight_device_non_edp(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	struct amdgpu_display_manager *dm;

	setup_test_connector(test, &fixture, -1, SIGNAL_TYPE_HDMI_TYPE_A);
	fixture.link->type = dc_connection_single;
	dm = &fixture.adev->dm;
	dm->adev = fixture.adev;
	dm->num_of_edps = 0;

	amdgpu_dm_setup_backlight_device(dm, fixture.aconnector);

	/* Non-eDP/LVDS signal → no backlight setup */
	KUNIT_EXPECT_EQ(test, dm->num_of_edps, 0);
	KUNIT_EXPECT_EQ(test, fixture.aconnector->bl_idx, -1);
}

/**
 * dm_test_setup_backlight_device_connection_none - Test disconnected link is skipped
 * @test: The KUnit test context
 */
static void dm_test_setup_backlight_device_connection_none(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	struct amdgpu_display_manager *dm;

	setup_test_connector(test, &fixture, -1, SIGNAL_TYPE_EDP);
	fixture.link->type = dc_connection_none;
	dm = &fixture.adev->dm;
	dm->adev = fixture.adev;
	dm->num_of_edps = 0;

	amdgpu_dm_setup_backlight_device(dm, fixture.aconnector);

	/* Disconnected link → no backlight setup */
	KUNIT_EXPECT_EQ(test, dm->num_of_edps, 0);
	KUNIT_EXPECT_EQ(test, fixture.aconnector->bl_idx, -1);
}

/**
 * dm_test_setup_backlight_device_max_edps - Test setup is skipped when at eDP limit
 * @test: The KUnit test context
 */
static void dm_test_setup_backlight_device_max_edps(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	struct amdgpu_display_manager *dm;

	setup_test_connector(test, &fixture, -1, SIGNAL_TYPE_EDP);
	fixture.link->type = dc_connection_single;
	dm = &fixture.adev->dm;
	dm->adev = fixture.adev;
	dm->num_of_edps = AMDGPU_DM_MAX_NUM_EDP;

	amdgpu_dm_setup_backlight_device(dm, fixture.aconnector);

	/* Already at the eDP limit → no additional setup */
	KUNIT_EXPECT_EQ(test, dm->num_of_edps, AMDGPU_DM_MAX_NUM_EDP);
	KUNIT_EXPECT_EQ(test, fixture.aconnector->bl_idx, -1);
}

/**
 * dm_test_setup_backlight_device_oled_success - Test successful eDP backlight setup
 * @test: The KUnit test context
 */
static void dm_test_setup_backlight_device_oled_success(struct kunit *test)
{
	struct dm_backlight_connector_fixture fixture = {};
	struct amdgpu_display_manager *dm;
	int saved_backlight = amdgpu_dm_get_backlight_param();

	amdgpu_dm_set_backlight_param(-1);
	setup_test_connector(test, &fixture, -1, SIGNAL_TYPE_EDP);
	fixture.link->type = dc_connection_single;
	/* OLED panel avoids the ABM property attach path */
	fixture.link->dpcd_sink_ext_caps.bits.oled = 1;
	dm = &fixture.adev->dm;
	dm->adev = fixture.adev;
	dm->num_of_edps = 0;

	amdgpu_dm_setup_backlight_device(dm, fixture.aconnector);

	KUNIT_EXPECT_EQ(test, dm->num_of_edps, 1);
	KUNIT_EXPECT_EQ(test, fixture.aconnector->bl_idx, 0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)dm->backlight_link[0],
			    (void *)fixture.link);
	KUNIT_EXPECT_TRUE(test, dm->backlight_caps[0].aux_support);

	amdgpu_dm_set_backlight_param(saved_backlight);
}

static struct kunit_case dm_backlight_test_cases[] = {
	KUNIT_CASE(dm_test_backlight_caps_valid_short_circuit),
#if !defined(CONFIG_ACPI)
	KUNIT_CASE(dm_test_backlight_caps_aux_support_noop),
	KUNIT_CASE(dm_test_backlight_caps_non_aux_sets_defaults),
#endif
	/* get_brightness_range */
	KUNIT_CASE(dm_test_brightness_range_null_caps),
	KUNIT_CASE(dm_test_brightness_range_pwm),
	KUNIT_CASE(dm_test_brightness_range_aux),
	/* convert_brightness_to_user */
	KUNIT_CASE(dm_test_brightness_to_user_null_caps),
	KUNIT_CASE(dm_test_brightness_to_user_below_min),
	KUNIT_CASE(dm_test_brightness_to_user_at_max),
	KUNIT_CASE(dm_test_brightness_to_user_at_min),
	KUNIT_CASE(dm_test_brightness_to_user_midpoint_pwm),
	/* convert_brightness_from_user */
	KUNIT_CASE(dm_test_brightness_from_user_null_caps),
	KUNIT_CASE(dm_test_brightness_from_user_zero),
	KUNIT_CASE(dm_test_brightness_from_user_max),
	KUNIT_CASE(dm_test_brightness_from_user_aux),
	/* convert_custom_brightness */
	KUNIT_CASE(dm_test_custom_brightness_no_data_points),
	KUNIT_CASE(dm_test_custom_brightness_debug_mask_disables),
	KUNIT_CASE(dm_test_custom_brightness_exact_match),
	KUNIT_CASE(dm_test_custom_brightness_below_first),
	KUNIT_CASE(dm_test_custom_brightness_interpolation),
	KUNIT_CASE(dm_test_custom_brightness_above_last),
	KUNIT_CASE(dm_test_custom_brightness_single_data_point),
	KUNIT_CASE(dm_test_custom_brightness_lower_lum_zero),
	KUNIT_CASE(dm_test_brightness_to_user_above_max),
	KUNIT_CASE(dm_test_brightness_from_user_midrange),
	KUNIT_CASE(dm_test_brightness_from_user_with_curve),
	KUNIT_CASE(dm_test_brightness_range_zero_signals),
	/* amdgpu_dm_update_connector_ext_caps */
	KUNIT_CASE(dm_test_update_connector_ext_caps_negative_bl_idx),
	KUNIT_CASE(dm_test_update_connector_ext_caps_non_edp),
	KUNIT_CASE(dm_test_update_connector_ext_caps_oled_defaults),
	KUNIT_CASE(dm_test_update_connector_ext_caps_luminance_values),
	KUNIT_CASE(dm_test_update_connector_ext_caps_force_aux),
	KUNIT_CASE(dm_test_update_connector_ext_caps_force_pwm),
	/* amdgpu_dm_should_create_sysfs */
	KUNIT_CASE(dm_test_should_create_sysfs_abm_forced),
	KUNIT_CASE(dm_test_should_create_sysfs_non_edp),
	KUNIT_CASE(dm_test_should_create_sysfs_no_backlight_index),
	KUNIT_CASE(dm_test_should_create_sysfs_aux_backlight),
	KUNIT_CASE(dm_test_should_create_sysfs_pwm_backlight),
	/* amdgpu_dm_setup_backlight_device */
	KUNIT_CASE(dm_test_setup_backlight_device_non_edp),
	KUNIT_CASE(dm_test_setup_backlight_device_connection_none),
	KUNIT_CASE(dm_test_setup_backlight_device_max_edps),
	KUNIT_CASE(dm_test_setup_backlight_device_oled_success),
	{}
};

static struct kunit_suite dm_backlight_test_suite = {
	.name = "amdgpu_dm_backlight",
	.test_cases = dm_backlight_test_cases,
};

kunit_test_suite(dm_backlight_test_suite);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_backlight");
MODULE_AUTHOR("AMD");
