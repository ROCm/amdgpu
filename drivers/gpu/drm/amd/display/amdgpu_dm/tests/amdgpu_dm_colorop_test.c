// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_colorop.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <drm/drm_colorop.h>

#include "amdgpu_dm_colorop.h"

/* Tests for amdgpu_dm_supported_degam_tfs */

static void dm_test_supported_degam_tfs_has_srgb_eotf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_degam_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_SRGB_EOTF));
}

static void dm_test_supported_degam_tfs_has_pq125_eotf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_degam_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_PQ_125_EOTF));
}

static void dm_test_supported_degam_tfs_has_bt2020_inv_oetf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_degam_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_BT2020_INV_OETF));
}

static void dm_test_supported_degam_tfs_has_gamma22_inv(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_degam_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_GAMMA22_INV));
}

static void dm_test_supported_degam_tfs_no_extra_bits(struct kunit *test)
{
	u64 expected = BIT(DRM_COLOROP_1D_CURVE_SRGB_EOTF) |
		       BIT(DRM_COLOROP_1D_CURVE_PQ_125_EOTF) |
		       BIT(DRM_COLOROP_1D_CURVE_BT2020_INV_OETF) |
		       BIT(DRM_COLOROP_1D_CURVE_GAMMA22_INV);

	KUNIT_EXPECT_EQ(test, amdgpu_dm_supported_degam_tfs, expected);
}

/* Tests for amdgpu_dm_supported_shaper_tfs */

static void dm_test_supported_shaper_tfs_has_srgb_inv_eotf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_shaper_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_SRGB_INV_EOTF));
}

static void dm_test_supported_shaper_tfs_has_pq125_inv_eotf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_shaper_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_PQ_125_INV_EOTF));
}

static void dm_test_supported_shaper_tfs_has_bt2020_oetf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_shaper_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_BT2020_OETF));
}

static void dm_test_supported_shaper_tfs_has_gamma22(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_shaper_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_GAMMA22));
}

static void dm_test_supported_shaper_tfs_no_extra_bits(struct kunit *test)
{
	u64 expected = BIT(DRM_COLOROP_1D_CURVE_SRGB_INV_EOTF) |
		       BIT(DRM_COLOROP_1D_CURVE_PQ_125_INV_EOTF) |
		       BIT(DRM_COLOROP_1D_CURVE_BT2020_OETF) |
		       BIT(DRM_COLOROP_1D_CURVE_GAMMA22);

	KUNIT_EXPECT_EQ(test, amdgpu_dm_supported_shaper_tfs, expected);
}

/* Tests for amdgpu_dm_supported_blnd_tfs */

static void dm_test_supported_blnd_tfs_has_srgb_eotf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_blnd_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_SRGB_EOTF));
}

static void dm_test_supported_blnd_tfs_has_pq125_eotf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_blnd_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_PQ_125_EOTF));
}

static void dm_test_supported_blnd_tfs_has_bt2020_inv_oetf(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_blnd_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_BT2020_INV_OETF));
}

static void dm_test_supported_blnd_tfs_has_gamma22_inv(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_supported_blnd_tfs &
			  BIT(DRM_COLOROP_1D_CURVE_GAMMA22_INV));
}

static void dm_test_supported_blnd_tfs_no_extra_bits(struct kunit *test)
{
	u64 expected = BIT(DRM_COLOROP_1D_CURVE_SRGB_EOTF) |
		       BIT(DRM_COLOROP_1D_CURVE_PQ_125_EOTF) |
		       BIT(DRM_COLOROP_1D_CURVE_BT2020_INV_OETF) |
		       BIT(DRM_COLOROP_1D_CURVE_GAMMA22_INV);

	KUNIT_EXPECT_EQ(test, amdgpu_dm_supported_blnd_tfs, expected);
}

/* degam and blnd should support the same set of EOTF curves */
static void dm_test_degam_and_blnd_tfs_match(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, amdgpu_dm_supported_degam_tfs,
			amdgpu_dm_supported_blnd_tfs);
}

static struct kunit_case dm_colorop_test_cases[] = {
	/* degam TFs */
	KUNIT_CASE(dm_test_supported_degam_tfs_has_srgb_eotf),
	KUNIT_CASE(dm_test_supported_degam_tfs_has_pq125_eotf),
	KUNIT_CASE(dm_test_supported_degam_tfs_has_bt2020_inv_oetf),
	KUNIT_CASE(dm_test_supported_degam_tfs_has_gamma22_inv),
	KUNIT_CASE(dm_test_supported_degam_tfs_no_extra_bits),
	/* shaper TFs */
	KUNIT_CASE(dm_test_supported_shaper_tfs_has_srgb_inv_eotf),
	KUNIT_CASE(dm_test_supported_shaper_tfs_has_pq125_inv_eotf),
	KUNIT_CASE(dm_test_supported_shaper_tfs_has_bt2020_oetf),
	KUNIT_CASE(dm_test_supported_shaper_tfs_has_gamma22),
	KUNIT_CASE(dm_test_supported_shaper_tfs_no_extra_bits),
	/* blnd TFs */
	KUNIT_CASE(dm_test_supported_blnd_tfs_has_srgb_eotf),
	KUNIT_CASE(dm_test_supported_blnd_tfs_has_pq125_eotf),
	KUNIT_CASE(dm_test_supported_blnd_tfs_has_bt2020_inv_oetf),
	KUNIT_CASE(dm_test_supported_blnd_tfs_has_gamma22_inv),
	KUNIT_CASE(dm_test_supported_blnd_tfs_no_extra_bits),
	/* cross-check */
	KUNIT_CASE(dm_test_degam_and_blnd_tfs_match),
	{}
};

static struct kunit_suite dm_colorop_test_suite = {
	.name = "amdgpu_dm_colorop",
	.test_cases = dm_colorop_test_cases,
};

kunit_test_suite(dm_colorop_test_suite);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_colorop");
MODULE_AUTHOR("AMD");
