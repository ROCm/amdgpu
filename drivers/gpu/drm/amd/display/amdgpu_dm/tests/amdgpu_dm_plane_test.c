// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_plane.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <drm/drm_atomic.h>
#include <drm/drm_blend.h>
#include "link_enc_cfg.h"
#include "amdgpu_dm_plane.h"
#include "amdgpu_rlc.h"
#include "gc/gc_11_0_0_offset.h"
#include "gc/gc_11_0_0_sh_mask.h"
#include <drm/amdgpu_drm.h>
#include <drm/drm_plane.h>

struct dm_test_dcc_cap_ctx {
	bool callback_ret;
	bool capable;
	bool output_independent_64b_blks;
	bool called;
	struct dc_dcc_surface_param captured_input;
};

static struct dm_test_dcc_cap_ctx *dm_test_dcc_ctx;

struct dm_test_gfx11_reg_ctx {
	u32 gb_addr_config;
	u32 gc_reg_offsets[1];
	u32 expected_reg;
	u32 captured_reg;
	u32 captured_acc_flags;
	u32 captured_hwip;
	u32 captured_xcc_id;
	bool called;
};

static struct dm_test_gfx11_reg_ctx *dm_test_gfx11_reg_ctx;

static u32 dm_test_gfx11_rreg32(struct amdgpu_device *adev,
				u32 reg, u32 acc_flags, u32 hwip,
				u32 xcc_id)
{
	if (!dm_test_gfx11_reg_ctx)
		return 0;

	dm_test_gfx11_reg_ctx->called = true;
	dm_test_gfx11_reg_ctx->captured_reg = reg;
	dm_test_gfx11_reg_ctx->captured_acc_flags = acc_flags;
	dm_test_gfx11_reg_ctx->captured_hwip = hwip;
	dm_test_gfx11_reg_ctx->captured_xcc_id = xcc_id;

	return dm_test_gfx11_reg_ctx->gb_addr_config;
}

static const struct amdgpu_rlc_reg_funcs dm_test_gfx11_reg_funcs = {
	.rreg32 = dm_test_gfx11_rreg32,
};

static bool dm_test_get_dcc_compression_cap(const struct dc *dc,
					    const struct dc_dcc_surface_param *input,
					    struct dc_surface_dcc_cap *output)
{
	if (!dm_test_dcc_ctx)
		return false;

	dm_test_dcc_ctx->called = true;
	dm_test_dcc_ctx->captured_input = *input;
	output->capable = dm_test_dcc_ctx->capable;
	output->grph.rgb.independent_64b_blks = dm_test_dcc_ctx->output_independent_64b_blks;

	return dm_test_dcc_ctx->callback_ret;
}

static void dm_test_init_validate_dcc_inputs(struct amdgpu_device **adev,
					     struct dc **dc,
					     struct dc_tiling_info *tiling_info,
					     struct dc_plane_dcc_param *dcc,
					     struct dc_plane_address *address,
					     struct plane_size *plane_size,
					     struct kunit *test)
{
	*adev = kunit_kzalloc(test, sizeof(**adev), GFP_KERNEL);
	*dc = kunit_kzalloc(test, sizeof(**dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, *adev);
	KUNIT_ASSERT_NOT_NULL(test, *dc);

	(*adev)->dm.dc = *dc;
	(*adev)->family = AMDGPU_FAMILY_NV;

	tiling_info->gfx9.swizzle = 9;
	dcc->enable = 1;
	dcc->independent_64b_blks = 1;
	plane_size->surface_size.width = 1920;
	plane_size->surface_size.height = 1080;

	(void)address;
}


/**
 * dm_test_plane_is_video_format_known_video() - Verify known video formats.
 * @test: KUnit test context.
 *
 * Verify if NV12, NV21, and P010 are treated as video formats.
 */
static void dm_test_plane_is_video_format_known_video(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_plane_is_video_format(DRM_FORMAT_NV12));
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_plane_is_video_format(DRM_FORMAT_NV21));
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_plane_is_video_format(DRM_FORMAT_P010));
}

/**
 * dm_test_fill_blending_defaults() - Verify default blending output values.
 * @test: KUnit test context.
 *
 * Verify if default blending output values are used for opaque alpha and no
 * per-pixel blending.
 */
static void dm_test_fill_blending_defaults(struct kunit *test)
{
	struct drm_plane_state state = { 0 };
	bool per_pixel_alpha;
	bool pre_multiplied_alpha;
	bool global_alpha;
	int global_alpha_value;

	state.pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
	state.alpha = 0xffff;

	amdgpu_dm_plane_fill_blending_from_plane_state(&state,
						       &per_pixel_alpha,
						       &pre_multiplied_alpha,
						       &global_alpha,
						       &global_alpha_value);

	KUNIT_EXPECT_FALSE(test, per_pixel_alpha);
	KUNIT_EXPECT_TRUE(test, pre_multiplied_alpha);
	KUNIT_EXPECT_FALSE(test, global_alpha);
	KUNIT_EXPECT_EQ(test, global_alpha_value, 0xff);
}

/**
 * dm_test_fill_blending_premulti_alpha_format() - Verify premultiplied alpha path.
 * @test: KUnit test context.
 *
 * Verify if premultiplied mode enables per-pixel alpha for ARGB8888.
 */
static void dm_test_fill_blending_premulti_alpha_format(struct kunit *test)
{
	struct drm_plane_state state = { 0 };
	struct drm_framebuffer fb = { 0 };
	bool per_pixel_alpha;
	bool pre_multiplied_alpha;
	bool global_alpha;
	int global_alpha_value;

	fb.format = drm_format_info(DRM_FORMAT_ARGB8888);
	KUNIT_ASSERT_NOT_NULL(test, fb.format);

	state.fb = &fb;
	state.pixel_blend_mode = DRM_MODE_BLEND_PREMULTI;
	state.alpha = 0xffff;

	amdgpu_dm_plane_fill_blending_from_plane_state(&state,
						       &per_pixel_alpha,
						       &pre_multiplied_alpha,
						       &global_alpha,
						       &global_alpha_value);

	KUNIT_EXPECT_TRUE(test, per_pixel_alpha);
	KUNIT_EXPECT_TRUE(test, pre_multiplied_alpha);
	KUNIT_EXPECT_FALSE(test, global_alpha);
	KUNIT_EXPECT_EQ(test, global_alpha_value, 0xff);
}

/**
 * dm_test_fill_blending_coverage_alpha_format() - Verify coverage mode behavior.
 * @test: KUnit test context.
 *
 * Verify if coverage mode sets per-pixel alpha and disables
 * pre_multiplied_alpha for ARGB8888.
 */
static void dm_test_fill_blending_coverage_alpha_format(struct kunit *test)
{
	struct drm_plane_state state = { 0 };
	struct drm_framebuffer fb = { 0 };
	bool per_pixel_alpha;
	bool pre_multiplied_alpha;
	bool global_alpha;
	int global_alpha_value;

	fb.format = drm_format_info(DRM_FORMAT_ARGB8888);
	KUNIT_ASSERT_NOT_NULL(test, fb.format);

	state.fb = &fb;
	state.pixel_blend_mode = DRM_MODE_BLEND_COVERAGE;
	state.alpha = 0xffff;

	amdgpu_dm_plane_fill_blending_from_plane_state(&state,
						       &per_pixel_alpha,
						       &pre_multiplied_alpha,
						       &global_alpha,
						       &global_alpha_value);

	KUNIT_EXPECT_TRUE(test, per_pixel_alpha);
	KUNIT_EXPECT_FALSE(test, pre_multiplied_alpha);
	KUNIT_EXPECT_FALSE(test, global_alpha);
	KUNIT_EXPECT_EQ(test, global_alpha_value, 0xff);
}

/**
 * dm_test_fill_blending_global_alpha() - Verify global alpha conversion to 8 bits.
 * @test: KUnit test context.
 *
 * Verify if global alpha is enabled and converted from 16-bit to 8-bit.
 */
static void dm_test_fill_blending_global_alpha(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct drm_plane *plane;
	struct drm_plane_state *state;
	bool per_pixel_alpha;
	bool pre_multiplied_alpha;
	bool global_alpha;
	int global_alpha_value;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);

	plane->dev = &adev->ddev;
	state->plane = plane;
	state->pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
	state->alpha = 0x8000;

	amdgpu_dm_plane_fill_blending_from_plane_state(state,
						       &per_pixel_alpha,
						       &pre_multiplied_alpha,
						       &global_alpha,
						       &global_alpha_value);

	KUNIT_EXPECT_FALSE(test, per_pixel_alpha);
	KUNIT_EXPECT_TRUE(test, pre_multiplied_alpha);
	KUNIT_EXPECT_TRUE(test, global_alpha);
	KUNIT_EXPECT_EQ(test, global_alpha_value, 0x80);
}

/**
 * dm_test_modifier_has_dcc() - Verify helper detects AMD DCC modifiers.
 * @test: KUnit test context.
 *
 * Verify if DCC detection works for linear and AMD DCC modifiers.
 */
static void dm_test_modifier_has_dcc(struct kunit *test)
{
	uint64_t dcc_mod = AMD_FMT_MOD | AMD_FMT_MOD_SET(DCC, 1);

	KUNIT_EXPECT_FALSE(test, amdgpu_dm_plane_modifier_has_dcc(DRM_FORMAT_MOD_LINEAR));
	KUNIT_EXPECT_TRUE(test, amdgpu_dm_plane_modifier_has_dcc(dcc_mod));
}

/**
 * dm_test_modifier_gfx9_swizzle_mode() - Verify swizzle helper for linear and AMD modifiers.
 * @test: KUnit test context.
 *
 * Verify if swizzle mode decoding works for linear and AMD tiled modifiers.
 */
static void dm_test_modifier_gfx9_swizzle_mode(struct kunit *test)
{
	uint64_t mod = AMD_FMT_MOD | AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X);

	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_modifier_gfx9_swizzle_mode(DRM_FORMAT_MOD_LINEAR), 0U);
	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_modifier_gfx9_swizzle_mode(mod),
			(unsigned int)AMD_FMT_MOD_TILE_GFX9_64K_S_X);
}

/**
 * dm_test_get_plane_formats() - Verify plane format counts for key plane types.
 * @test: KUnit test context.
 *
 * Verify if returned format counts match primary, overlay, and cursor planes.
 */
static void dm_test_get_plane_formats(struct kunit *test)
{
	struct drm_plane *plane;
	struct dc_plane_cap *cap;
	uint32_t formats[32] = {0};

	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	cap = kunit_kzalloc(test, sizeof(*cap), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, cap);

	plane->type = DRM_PLANE_TYPE_PRIMARY;
	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_get_plane_formats(plane, NULL, formats, 32), 14);

	cap->pixel_format_support.nv12 = true;
	cap->pixel_format_support.p010 = true;
	cap->pixel_format_support.fp16 = true;
	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_get_plane_formats(plane, cap, formats, 32), 20);

	plane->type = DRM_PLANE_TYPE_OVERLAY;
	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_get_plane_formats(plane, NULL, formats, 32), 9);

	plane->type = DRM_PLANE_TYPE_CURSOR;
	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_get_plane_formats(plane, NULL, formats, 32), 1);
}

/**
 * dm_test_get_plane_modifiers() - Verify early-return and cursor modifier list.
 * @test: KUnit test context.
 *
 * Verify if modifier list handling works for unsupported families and cursor planes.
 */
static void dm_test_get_plane_modifiers(struct kunit *test)
{
	struct amdgpu_device *adev;
	uint64_t *mods = NULL;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_SI;
	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_get_plane_modifiers(adev, DRM_PLANE_TYPE_PRIMARY, &mods),
			0);
	KUNIT_EXPECT_PTR_EQ(test, mods, NULL);

	adev->family = AMDGPU_FAMILY_NV;
	KUNIT_ASSERT_EQ(test,
			amdgpu_dm_plane_get_plane_modifiers(adev, DRM_PLANE_TYPE_CURSOR, &mods),
			0);
	KUNIT_ASSERT_NOT_NULL(test, mods);
	KUNIT_EXPECT_EQ(test, mods[0], DRM_FORMAT_MOD_LINEAR);
	KUNIT_EXPECT_EQ(test, mods[1], DRM_FORMAT_MOD_INVALID);
	kfree(mods);
}

/**
 * dm_test_fill_dc_scaling_info() - Verify basic error and success paths.
 * @test: KUnit test context.
 *
 * Verify if scaling info rejects invalid sizes and accepts valid sizes.
 */
static void dm_test_fill_dc_scaling_info(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct drm_plane_state state = {0};
	struct dc_scaling_info info = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	state.src_w = 0;
	state.src_h = 100 << 16;
	state.crtc_w = 100;
	state.crtc_h = 100;
	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_fill_dc_scaling_info(adev, &state, &info), -EINVAL);

	state.src_w = 100 << 16;
	state.src_h = 100 << 16;
	state.crtc_w = 100;
	state.crtc_h = 100;
	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_fill_dc_scaling_info(adev, &state, &info), 0);
}

/**
 * dm_test_get_min_max_dc_plane_scaling() - Verify format-specific cap selection and 1->1000 conversion.
 * @test: KUnit test context.
 *
 * Verify if min/max scaling values are correct for NV12 and XRGB8888 formats.
 */
static void dm_test_get_min_max_dc_plane_scaling(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct drm_framebuffer *fb;
	int min_downscale = 0;
	int max_upscale = 0;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	adev->dm.dc = dc;
	dc->caps.planes[0].max_upscale_factor.nv12 = 1;
	dc->caps.planes[0].max_downscale_factor.nv12 = 1;
	dc->caps.planes[0].max_upscale_factor.argb8888 = 1600;
	dc->caps.planes[0].max_downscale_factor.argb8888 = 250;

	fb->format = drm_format_info(DRM_FORMAT_NV12);
	KUNIT_ASSERT_NOT_NULL(test, fb->format);
	amdgpu_dm_plane_get_min_max_dc_plane_scaling(&adev->ddev, fb, &min_downscale, &max_upscale);
	KUNIT_EXPECT_EQ(test, min_downscale, 1000);
	KUNIT_EXPECT_EQ(test, max_upscale, 1000);

	fb->format = drm_format_info(DRM_FORMAT_XRGB8888);
	KUNIT_ASSERT_NOT_NULL(test, fb->format);
	amdgpu_dm_plane_get_min_max_dc_plane_scaling(&adev->ddev, fb, &min_downscale, &max_upscale);
	KUNIT_EXPECT_EQ(test, min_downscale, 250);
	KUNIT_EXPECT_EQ(test, max_upscale, 1600);
}

/**
 * dm_test_fill_plane_buffer_attributes_gfx8() - Verify graphics path and GFX8 tiling fill.
 * @test: KUnit test context.
 *
 * Verify if GFX8 plane buffer attributes and tiling fields are filled correctly.
 */
static void dm_test_fill_plane_buffer_attributes_gfx8(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_framebuffer *afb;
	struct dc_tiling_info *tiling_info;
	struct plane_size *plane_size;
	struct dc_plane_dcc_param *dcc;
	struct dc_plane_address *address;
	uint64_t tiling_flags = 0;
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	tiling_info = kunit_kzalloc(test, sizeof(*tiling_info), GFP_KERNEL);
	plane_size = kunit_kzalloc(test, sizeof(*plane_size), GFP_KERNEL);
	dcc = kunit_kzalloc(test, sizeof(*dcc), GFP_KERNEL);
	address = kunit_kzalloc(test, sizeof(*address), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, afb);
	KUNIT_ASSERT_NOT_NULL(test, tiling_info);
	KUNIT_ASSERT_NOT_NULL(test, plane_size);
	KUNIT_ASSERT_NOT_NULL(test, dcc);
	KUNIT_ASSERT_NOT_NULL(test, address);

	adev->family = AMDGPU_FAMILY_SI;
	afb->address = 0x12345000ULL;
	afb->base.width = 1920;
	afb->base.height = 1080;
	afb->base.offsets[0] = 0x1000;
	afb->base.pitches[0] = 7680;
	afb->base.format = drm_format_info(DRM_FORMAT_XRGB8888);
	KUNIT_ASSERT_NOT_NULL(test, afb->base.format);

	tiling_flags |= AMDGPU_TILING_SET(ARRAY_MODE, DC_ARRAY_1D_TILED_THIN1);
	tiling_flags |= AMDGPU_TILING_SET(PIPE_CONFIG, 5);

	ret = amdgpu_dm_plane_fill_plane_buffer_attributes(adev, afb,
		SURFACE_PIXEL_FORMAT_GRPH_ARGB8888, ROTATION_ANGLE_0,
		tiling_flags, tiling_info, plane_size, dcc, address, true);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, plane_size->surface_size.width, 1920);
	KUNIT_EXPECT_EQ(test, plane_size->surface_size.height, 1080);
	KUNIT_EXPECT_EQ(test, plane_size->surface_pitch, 1920);
	KUNIT_EXPECT_EQ(test, address->type, (int)PLN_ADDR_TYPE_GRAPHICS);
	KUNIT_EXPECT_TRUE(test, address->tmz_surface);
	KUNIT_EXPECT_EQ(test, (int)tiling_info->gfx8.array_mode, (int)DC_ARRAY_1D_TILED_THIN1);
	KUNIT_EXPECT_EQ(test, tiling_info->gfx8.pipe_config, 5U);
}

/**
 * dm_test_get_cursor_position() - Verify cursor clipping and off-screen handling.
 * @test: KUnit test context.
 *
 * Verify if cursor clipping, hotspot adjustment, and off-screen disable behavior work.
 */
static void dm_test_get_cursor_position(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *amdgpu_crtc;
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_framebuffer *fb;
	struct dc_cursor_position position = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	amdgpu_crtc = kunit_kzalloc(test, sizeof(*amdgpu_crtc), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, amdgpu_crtc);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	adev->ip_versions[DCE_HWIP][0] = IP_VERSION(4, 0, 0);
	amdgpu_crtc->max_cursor_width = 64;
	amdgpu_crtc->max_cursor_height = 64;

	plane->dev = &adev->ddev;
	plane->state = state;
	state->fb = fb;
	state->crtc_x = -5;
	state->crtc_y = -7;
	state->crtc_w = 32;
	state->crtc_h = 32;

	KUNIT_ASSERT_EQ(test,
			amdgpu_dm_plane_get_cursor_position(plane, &amdgpu_crtc->base, &position),
			0);
	KUNIT_EXPECT_TRUE(test, position.enable);
	KUNIT_EXPECT_EQ(test, position.x, 0);
	KUNIT_EXPECT_EQ(test, position.y, 0);
	KUNIT_EXPECT_EQ(test, position.x_hotspot, 5);
	KUNIT_EXPECT_EQ(test, position.y_hotspot, 7);
	KUNIT_EXPECT_TRUE(test, position.translate_by_source);

	memset(&position, 0, sizeof(position));
	state->crtc_x = -64;
	state->crtc_y = 0;
	KUNIT_ASSERT_EQ(test,
			amdgpu_dm_plane_get_cursor_position(plane, &amdgpu_crtc->base, &position),
			0);
	KUNIT_EXPECT_FALSE(test, position.enable);
}

/**
 * dm_test_format_mod_supported() - Verify key format/modifier acceptance and rejection paths.
 * @test: KUnit test context.
 *
 * Verify if format-modifier support checks match accepted and rejected cases.
 */
static void dm_test_format_mod_supported(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct drm_plane *plane;
	uint64_t listed_mod;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, plane);

	adev->family = AMDGPU_FAMILY_NV;
	plane->dev = &adev->ddev;

	KUNIT_EXPECT_TRUE(test,
			  amdgpu_dm_plane_format_mod_supported(plane, DRM_FORMAT_XRGB8888,
							       DRM_FORMAT_MOD_LINEAR));
	KUNIT_EXPECT_TRUE(test,
			  amdgpu_dm_plane_format_mod_supported(plane, DRM_FORMAT_XRGB8888,
							       DRM_FORMAT_MOD_INVALID));

	KUNIT_EXPECT_FALSE(test,
			   amdgpu_dm_plane_format_mod_supported(plane, DRM_FORMAT_XRGB8888,
								DRM_FORMAT_MOD_VENDOR_AMD));

	listed_mod = AMD_FMT_MOD |
		     AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
		     AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX9) |
		     AMD_FMT_MOD_SET(DCC, 1);
	plane->modifiers = &listed_mod;
	plane->modifier_count = 1;

	KUNIT_EXPECT_FALSE(test,
			   amdgpu_dm_plane_format_mod_supported(plane, DRM_FORMAT_NV12, listed_mod));
}

/**
 * dm_test_fill_gfx12_plane_attributes_from_modifiers() - Verify GFX12 DCC mapping path.
 * @test: KUnit test context.
 *
 * Verify if GFX12 modifier parsing enables DCC and sets expected DCC block mode.
 */
static void dm_test_fill_gfx12_plane_attributes_from_modifiers(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = true,
		.capable = true,
		.output_independent_64b_blks = false,
	};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_GC_12_0_0;
	adev->dm.dc = dc;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 2;
	adev->gfx.config.gb_addr_config_fields.num_banks = 4;
	adev->gfx.config.gb_addr_config_fields.pipe_interleave_size = 256;
	adev->gfx.config.gb_addr_config_fields.num_se = 1;
	adev->gfx.config.gb_addr_config_fields.max_compress_frags = 2;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 1;
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	afb->base.modifier = AMD_FMT_MOD |
			     AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX12_64K_2D) |
			     AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX12) |
			     AMD_FMT_MOD_SET(DCC, 1) |
			     AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, 1);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_fill_gfx12_plane_attributes_from_modifiers(
			adev, afb, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
			ROTATION_ANGLE_0, &plane_size, &tiling_info, &dcc, &address),
			0);
	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfxversion, (int)DcGfxAddr3);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk, (int)hubp_ind_block_128b);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx9_plane_attributes_from_modifiers() - Verify basic GFX9 linear modifier path.
 * @test: KUnit test context.
 *
 * Verify if GFX9 linear modifier handling keeps DCC disabled.
 */
static void dm_test_fill_gfx9_plane_attributes_from_modifiers(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_NV;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 2;
	adev->gfx.config.gb_addr_config_fields.num_banks = 4;
	adev->gfx.config.gb_addr_config_fields.pipe_interleave_size = 256;
	adev->gfx.config.gb_addr_config_fields.num_se = 1;
	adev->gfx.config.gb_addr_config_fields.max_compress_frags = 2;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 1;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 2;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);

	afb->base.modifier = DRM_FORMAT_MOD_LINEAR;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_fill_gfx9_plane_attributes_from_modifiers(
			adev, afb, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
			ROTATION_ANGLE_0, &plane_size, &tiling_info, &dcc, &address),
			0);
	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfxversion, (int)DcGfxVersion9);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.swizzle, 0U);
	KUNIT_EXPECT_FALSE(test, dcc.enable);
}

/**
 * dm_test_helper_check_state_viewport_reject() - Verify viewport outside screen rejects state.
 * @test: KUnit test context.
 *
 * Verify if plane state is rejected when the viewport is outside display bounds.
 */
static void dm_test_helper_check_state_viewport_reject(struct kunit *test)
{
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_framebuffer *fb;

	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	crtc = kunit_kzalloc(test, sizeof(*crtc), GFP_KERNEL);
	new_crtc_state = kunit_kzalloc(test, sizeof(*new_crtc_state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, crtc);
	KUNIT_ASSERT_NOT_NULL(test, new_crtc_state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	plane->type = DRM_PLANE_TYPE_OVERLAY;
	state->plane = plane;
	state->fb = fb;
	state->crtc = crtc;
	state->crtc_x = 200;
	state->crtc_y = 0;
	state->crtc_w = 100;
	state->crtc_h = 100;
	new_crtc_state->mode.crtc_hdisplay = 100;
	new_crtc_state->mode.crtc_vdisplay = 100;

	KUNIT_EXPECT_EQ(test, amdgpu_dm_plane_helper_check_state(state, new_crtc_state), -EINVAL);
}

/**
 * dm_test_validate_dcc_disabled_returns_success() - Verify disabled DCC is accepted.
 * @test: KUnit test context.
 *
 * Verify if DCC validation succeeds when DCC is disabled.
 */
static void dm_test_validate_dcc_disabled_returns_success(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct plane_size plane_size = {0};

	dm_test_init_validate_dcc_inputs(&adev, &dc, &tiling_info, &dcc, &address,
					 &plane_size, test);
	dcc.enable = 0;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_validate_dcc(adev, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
			ROTATION_ANGLE_0, &tiling_info, &dcc,
			&address, &plane_size),
			0);
}

/**
 * dm_test_validate_dcc_video_non_gfx12_fails() - Verify video format restriction on pre-GFX12.
 * @test: KUnit test context.
 *
 * Verify if video format DCC validation fails on non-GFX12 devices.
 */
static void dm_test_validate_dcc_video_non_gfx12_fails(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct plane_size plane_size = {0};

	dm_test_init_validate_dcc_inputs(&adev, &dc, &tiling_info, &dcc, &address,
					 &plane_size, test);
	adev->family = AMDGPU_FAMILY_NV;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_validate_dcc(adev, SURFACE_PIXEL_FORMAT_VIDEO_420_YCbCr,
			ROTATION_ANGLE_0, &tiling_info, &dcc,
			&address, &plane_size),
			-EINVAL);
}

/**
 * dm_test_validate_dcc_missing_cap_func_fails() - Verify missing capability callback fails.
 * @test: KUnit test context.
 *
 * Verify if validation fails when DCC capability callback is not provided.
 */
static void dm_test_validate_dcc_missing_cap_func_fails(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct plane_size plane_size = {0};

	dm_test_init_validate_dcc_inputs(&adev, &dc, &tiling_info, &dcc, &address,
					 &plane_size, test);
	dc->cap_funcs.get_dcc_compression_cap = NULL;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_validate_dcc(adev, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
			ROTATION_ANGLE_0, &tiling_info, &dcc,
			&address, &plane_size),
			-EINVAL);
}

/**
 * dm_test_validate_dcc_cap_callback_fails() - Verify callback failure path.
 * @test: KUnit test context.
 *
 * Verify if validation fails when the DCC capability callback returns false.
 */
static void dm_test_validate_dcc_cap_callback_fails(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct plane_size plane_size = {0};
	enum surface_pixel_format format = SURFACE_PIXEL_FORMAT_GRPH_ARGB8888;
	enum dc_rotation_angle rotation = ROTATION_ANGLE_0;
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = false,
		.capable = true,
	};
	int ret;

	dm_test_init_validate_dcc_inputs(&adev, &dc, &tiling_info, &dcc, &address,
					 &plane_size, test);
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	ret = amdgpu_dm_plane_validate_dcc(adev, format, rotation, &tiling_info,
					   &dcc, &address, &plane_size);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_TRUE(test, ctx.called);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_validate_dcc_not_capable_fails() - Verify not-capable callback output.
 * @test: KUnit test context.
 *
 * Verify if validation fails when the DCC capability callback reports that the
 * surface is not DCC capable.
 */
static void dm_test_validate_dcc_not_capable_fails(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct plane_size plane_size = {0};
	enum surface_pixel_format format = SURFACE_PIXEL_FORMAT_GRPH_ARGB8888;
	enum dc_rotation_angle rotation = ROTATION_ANGLE_0;
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = true,
		.capable = false,
	};
	int ret;

	dm_test_init_validate_dcc_inputs(&adev, &dc, &tiling_info, &dcc, &address,
					 &plane_size, test);
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	ret = amdgpu_dm_plane_validate_dcc(adev, format, rotation, &tiling_info,
					   &dcc, &address, &plane_size);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_TRUE(test, ctx.called);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_validate_dcc_success_and_scan_mapping() - Verify success path and rotation-to-scan mapping.
 * @test: KUnit test context.
 *
 * Verify if DCC validation succeeds and rotation-to-scan mapping is correct.
 */
static void dm_test_validate_dcc_success_and_scan_mapping(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct plane_size plane_size = {0};
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = true,
		.capable = true,
		.output_independent_64b_blks = true,
	};

	dm_test_init_validate_dcc_inputs(&adev, &dc, &tiling_info, &dcc, &address,
					 &plane_size, test);
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_validate_dcc(adev, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
			ROTATION_ANGLE_90, &tiling_info, &dcc,
			&address, &plane_size),
			0);
	KUNIT_EXPECT_TRUE(test, ctx.called);
	KUNIT_EXPECT_EQ(test, (int)ctx.captured_input.scan, (int)SCAN_DIRECTION_VERTICAL);
	KUNIT_EXPECT_EQ(test, (int)ctx.captured_input.format,
			(int)SURFACE_PIXEL_FORMAT_GRPH_ARGB8888);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_validate_dcc_independent_64b_mismatch_fails() - Verify 64B compatibility check.
 * @test: KUnit test context.
 *
 * Verify if validation fails when independent_64b_blks values do not match.
 */
static void dm_test_validate_dcc_independent_64b_mismatch_fails(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct plane_size plane_size = {0};
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = true,
		.capable = true,
		.output_independent_64b_blks = true,
	};

	dm_test_init_validate_dcc_inputs(&adev, &dc, &tiling_info, &dcc, &address,
					 &plane_size, test);
	dcc.independent_64b_blks = 0;
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_validate_dcc(adev, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
			ROTATION_ANGLE_0, &tiling_info, &dcc,
			&address, &plane_size),
			-EINVAL);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_add_modifier_appends_value() - Verify one modifier append.
 * @test: KUnit test context.
 *
 * Verify if a modifier is appended and size is updated.
 */
static void dm_test_add_modifier_appends_value(struct kunit *test)
{
	uint64_t size = 0;
	uint64_t cap = 2;
	uint64_t *mods = kmalloc_array(cap, sizeof(*mods), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, mods);

	amdgpu_dm_plane_add_modifier(&mods, &size, &cap, 0x1234ULL);

	KUNIT_ASSERT_NOT_NULL(test, mods);
	KUNIT_EXPECT_EQ(test, size, 1ULL);
	KUNIT_EXPECT_EQ(test, cap, 2ULL);
	KUNIT_EXPECT_EQ(test, mods[0], 0x1234ULL);

	kfree(mods);
}

/**
 * dm_test_add_modifier_grows_capacity() - Verify add triggers growth and preserves old data.
 * @test: KUnit test context.
 *
 * Verify if modifier array growth keeps old data and appends new data.
 */
static void dm_test_add_modifier_grows_capacity(struct kunit *test)
{
	uint64_t size = 1;
	uint64_t cap = 1;
	uint64_t *mods = kmalloc_array(cap, sizeof(*mods), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, mods);
	mods[0] = 0xAAULL;

	amdgpu_dm_plane_add_modifier(&mods, &size, &cap, 0xBBULL);

	KUNIT_ASSERT_NOT_NULL(test, mods);
	KUNIT_EXPECT_EQ(test, cap, 2ULL);
	KUNIT_EXPECT_EQ(test, size, 2ULL);
	KUNIT_EXPECT_EQ(test, mods[0], 0xAAULL);
	KUNIT_EXPECT_EQ(test, mods[1], 0xBBULL);

	kfree(mods);
}

/**
 * dm_test_add_modifier_noop_when_mods_null() - Verify helper is a no-op on NULL mods list.
 * @test: KUnit test context.
 *
 * Verify if add_modifier does nothing when the modifier list is NULL.
 */
static void dm_test_add_modifier_noop_when_mods_null(struct kunit *test)
{
	uint64_t size = 3;
	uint64_t cap = 7;
	uint64_t *mods = NULL;

	amdgpu_dm_plane_add_modifier(&mods, &size, &cap, 0x55ULL);

	KUNIT_EXPECT_PTR_EQ(test, mods, NULL);
	KUNIT_EXPECT_EQ(test, size, 3ULL);
	KUNIT_EXPECT_EQ(test, cap, 7ULL);
}

/**
 * dm_test_fill_gfx8_tiling_info_2d_tiled() - Verify GFX8 2D tiled flag parsing.
 * @test: KUnit test context.
 *
 * Verify if 2D tiled GFX8 flags populate expected tiling fields.
 */
static void dm_test_fill_gfx8_tiling_info_2d_tiled(struct kunit *test)
{
	struct dc_tiling_info tiling_info = {0};
	uint64_t tiling_flags = 0;

	tiling_flags |= AMDGPU_TILING_SET(ARRAY_MODE, DC_ARRAY_2D_TILED_THIN1);
	tiling_flags |= AMDGPU_TILING_SET(BANK_WIDTH, 2);
	tiling_flags |= AMDGPU_TILING_SET(BANK_HEIGHT, 1);
	tiling_flags |= AMDGPU_TILING_SET(MACRO_TILE_ASPECT, 3);
	tiling_flags |= AMDGPU_TILING_SET(TILE_SPLIT, 4);
	tiling_flags |= AMDGPU_TILING_SET(NUM_BANKS, 2);
	tiling_flags |= AMDGPU_TILING_SET(PIPE_CONFIG, 7);

	amdgpu_dm_plane_fill_gfx8_tiling_info_from_flags(&tiling_info, tiling_flags);

	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfxversion, (int)DcGfxVersion8);
	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfx8.array_mode, (int)DC_ARRAY_2D_TILED_THIN1);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.bank_width, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.bank_height, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.tile_aspect, 3U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.tile_split, 4U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.num_banks, 2U);
	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfx8.tile_mode,
			(int)DC_ADDR_SURF_MICRO_TILING_DISPLAY);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.pipe_config, 7U);
}

/**
 * dm_test_fill_gfx8_tiling_info_1d_tiled() - Verify GFX8 1D tiled flag parsing.
 * @test: KUnit test context.
 *
 * Verify if 1D tiled GFX8 flags populate array mode and pipe config.
 */
static void dm_test_fill_gfx8_tiling_info_1d_tiled(struct kunit *test)
{
	struct dc_tiling_info tiling_info = {0};
	uint64_t tiling_flags = 0;

	tiling_flags |= AMDGPU_TILING_SET(ARRAY_MODE, DC_ARRAY_1D_TILED_THIN1);
	tiling_flags |= AMDGPU_TILING_SET(PIPE_CONFIG, 5);

	amdgpu_dm_plane_fill_gfx8_tiling_info_from_flags(&tiling_info, tiling_flags);

	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfx8.array_mode, (int)DC_ARRAY_1D_TILED_THIN1);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.pipe_config, 5U);
}

/**
 * dm_test_fill_gfx8_tiling_info_other_mode() - Verify non-1D/non-2D mode handling.
 * @test: KUnit test context.
 *
 * Verify if unsupported array mode keeps preset fields and updates pipe config.
 */
static void dm_test_fill_gfx8_tiling_info_other_mode(struct kunit *test)
{
	struct dc_tiling_info tiling_info = {0};
	uint64_t tiling_flags = 0;

	tiling_info.gfxversion = 0x7f;
	tiling_info.gfx8.array_mode = 0x7f;
	tiling_info.gfx8.tile_mode = 0x7f;
	tiling_info.gfx8.num_banks = 0x7f;

	tiling_flags |= AMDGPU_TILING_SET(PIPE_CONFIG, 6);

	amdgpu_dm_plane_fill_gfx8_tiling_info_from_flags(&tiling_info, tiling_flags);

	KUNIT_EXPECT_EQ(test, tiling_info.gfxversion, 0x7f);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.array_mode, 0x7f);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.tile_mode, 0x7f);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.num_banks, 0x7f);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx8.pipe_config, 6U);
}

/**
 * dm_test_fill_gfx9_tiling_info_from_device_pre_10_3() - Verify GFX9 field copy before 10.3.
 * @test: KUnit test context.
 *
 * Verify if pre-10.3 device fields are copied and existing num_pkrs is kept.
 */
static void dm_test_fill_gfx9_tiling_info_from_device_pre_10_3(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_tiling_info tiling_info = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->gfx.config.gb_addr_config_fields.num_banks = 8;
	adev->gfx.config.gb_addr_config_fields.pipe_interleave_size = 256;
	adev->gfx.config.gb_addr_config_fields.num_se = 2;
	adev->gfx.config.gb_addr_config_fields.max_compress_frags = 1;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 2;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 3;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 2, 9);

	tiling_info.gfx9.num_pkrs = 0x5a;

	amdgpu_dm_plane_fill_gfx9_tiling_info_from_device(adev, &tiling_info);

	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pipes, 4U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_banks, 8U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.pipe_interleave, 256U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_shader_engines, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.max_compressed_frags, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_rb_per_se, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.shaderEnable, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pkrs, 0x5aU);
}

/**
 * dm_test_fill_gfx9_tiling_info_from_device_10_3_plus() - Verify num_pkrs update on 10.3+.
 * @test: KUnit test context.
 *
 * Verify if 10.3+ device fields are copied and num_pkrs is updated.
 */
static void dm_test_fill_gfx9_tiling_info_from_device_10_3_plus(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_tiling_info tiling_info = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->gfx.config.gb_addr_config_fields.num_pipes = 2;
	adev->gfx.config.gb_addr_config_fields.num_banks = 4;
	adev->gfx.config.gb_addr_config_fields.pipe_interleave_size = 128;
	adev->gfx.config.gb_addr_config_fields.num_se = 1;
	adev->gfx.config.gb_addr_config_fields.max_compress_frags = 2;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 1;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 6;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);

	amdgpu_dm_plane_fill_gfx9_tiling_info_from_device(adev, &tiling_info);

	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pipes, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_banks, 4U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.pipe_interleave, 128U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_shader_engines, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.max_compressed_frags, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_rb_per_se, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.shaderEnable, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pkrs, 6U);
}

/**
 * dm_test_fill_gfx9_tiling_info_from_modifier_linear() - Verify non-AMD modifier keeps device values.
 * @test: KUnit test context.
 *
 * Verify if linear modifier path keeps values from device configuration.
 */
static void dm_test_fill_gfx9_tiling_info_from_modifier_linear(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_tiling_info tiling_info = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_NV;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->gfx.config.gb_addr_config_fields.num_banks = 8;
	adev->gfx.config.gb_addr_config_fields.pipe_interleave_size = 256;
	adev->gfx.config.gb_addr_config_fields.num_se = 2;
	adev->gfx.config.gb_addr_config_fields.max_compress_frags = 1;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 2;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 3;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);

	amdgpu_dm_plane_fill_gfx9_tiling_info_from_modifier(adev, &tiling_info,
							    DRM_FORMAT_MOD_LINEAR);

	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pipes, 4U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_banks, 8U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.pipe_interleave, 256U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_shader_engines, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.max_compressed_frags, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_rb_per_se, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.shaderEnable, 1U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pkrs, 3U);
}

/**
 * dm_test_fill_gfx9_tiling_info_from_modifier_pre_nv() - Verify AMD modifier updates banks on pre-NV.
 * @test: KUnit test context.
 *
 * Verify if AMD modifier updates pre-NV pipe, engine, and bank fields.
 */
static void dm_test_fill_gfx9_tiling_info_from_modifier_pre_nv(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_tiling_info tiling_info = {0};
	uint64_t modifier;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_RV;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->gfx.config.gb_addr_config_fields.num_banks = 16;
	adev->gfx.config.gb_addr_config_fields.pipe_interleave_size = 256;
	adev->gfx.config.gb_addr_config_fields.num_se = 2;
	adev->gfx.config.gb_addr_config_fields.max_compress_frags = 1;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 2;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 7;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 2, 9);

	tiling_info.gfx9.num_pkrs = 0x5a;

	modifier = AMD_FMT_MOD |
		    AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
		    AMD_FMT_MOD_SET(PIPE_XOR_BITS, 7) |
		    AMD_FMT_MOD_SET(BANK_XOR_BITS, 3) |
		    AMD_FMT_MOD_SET(PACKERS, 2);

	amdgpu_dm_plane_fill_gfx9_tiling_info_from_modifier(adev, &tiling_info, modifier);

	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pipes, 32U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_shader_engines, 4U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_banks, 8U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pkrs, 0x5aU);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.shaderEnable, 1U);
}

/**
 * dm_test_fill_gfx9_tiling_info_from_modifier_nv() - Verify AMD modifier updates packers on NV+.
 * @test: KUnit test context.
 *
 * Verify if AMD modifier updates NV+ pipe, engine, and packer fields.
 */
static void dm_test_fill_gfx9_tiling_info_from_modifier_nv(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_tiling_info tiling_info = {0};
	uint64_t modifier;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_NV;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 2;
	adev->gfx.config.gb_addr_config_fields.num_banks = 9;
	adev->gfx.config.gb_addr_config_fields.pipe_interleave_size = 128;
	adev->gfx.config.gb_addr_config_fields.num_se = 1;
	adev->gfx.config.gb_addr_config_fields.max_compress_frags = 2;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 1;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 2;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);

	modifier = AMD_FMT_MOD |
		    AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
		    AMD_FMT_MOD_SET(PIPE_XOR_BITS, 6) |
		    AMD_FMT_MOD_SET(BANK_XOR_BITS, 2) |
		    AMD_FMT_MOD_SET(PACKERS, 3);

	amdgpu_dm_plane_fill_gfx9_tiling_info_from_modifier(adev, &tiling_info, modifier);

	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pipes, 32U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_shader_engines, 2U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_banks, 9U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.num_pkrs, 8U);
	KUNIT_EXPECT_EQ(test, tiling_info.gfx9.shaderEnable, 1U);
}

/**
 * dm_test_get_format_info() - Verify modifier-based format info lookup.
 * @test: KUnit test context.
 *
 * Verify if non-AMD modifiers return NULL and AMD DCC modifiers resolve a
 * dedicated format info structure.
 */
static void dm_test_get_format_info(struct kunit *test)
{
	u64 dcc_mod = AMD_FMT_MOD |
		      AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX9) |
		      AMD_FMT_MOD_SET(DCC, 1);
	const struct drm_format_info *format_info;

	KUNIT_EXPECT_PTR_EQ(test,
			    (void *)amdgpu_dm_plane_get_format_info(DRM_FORMAT_XRGB8888,
								    DRM_FORMAT_MOD_LINEAR),
			    NULL);
	format_info = amdgpu_dm_plane_get_format_info(DRM_FORMAT_XRGB8888, dcc_mod);
	KUNIT_EXPECT_NOT_NULL(test, format_info);
}

/**
 * dm_test_fill_blending_global_alpha_dcn42() - Verify DCN 4.2 alpha scaling.
 * @test: KUnit test context.
 *
 * Verify if DCN 4.2 scales the 16-bit DRM alpha down by 4 bits instead of 8.
 */
static void dm_test_fill_blending_global_alpha_dcn42(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct drm_plane *plane;
	struct drm_plane_state *state;
	bool per_pixel_alpha;
	bool pre_multiplied_alpha;
	bool global_alpha;
	int global_alpha_value;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);

	adev->ip_versions[DCE_HWIP][0] = IP_VERSION(4, 2, 0);
	plane->dev = &adev->ddev;
	state->plane = plane;
	state->pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
	state->alpha = 0x8000;

	amdgpu_dm_plane_fill_blending_from_plane_state(state,
						       &per_pixel_alpha,
						       &pre_multiplied_alpha,
						       &global_alpha,
						       &global_alpha_value);

	KUNIT_EXPECT_TRUE(test, global_alpha);
	KUNIT_EXPECT_EQ(test, global_alpha_value, 0x800);
}

static void dm_test_expect_mods_terminated(struct kunit *test, struct amdgpu_device *adev)
{
	u64 *mods = NULL;
	int ret;
	int i;

	ret = amdgpu_dm_plane_get_plane_modifiers(adev, DRM_PLANE_TYPE_PRIMARY, &mods);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_NOT_NULL(test, mods);

	for (i = 0; mods[i] != DRM_FORMAT_MOD_INVALID; i++)
		;

	KUNIT_EXPECT_GT(test, i, 0);
	KUNIT_EXPECT_EQ(test, mods[i - 1], DRM_FORMAT_MOD_LINEAR);
	kfree(mods);
}

static bool dm_test_mods_contain(const u64 *mods, u64 expected)
{
	int i;

	for (i = 0; mods[i] != DRM_FORMAT_MOD_INVALID; i++) {
		if (mods[i] == expected)
			return true;
	}

	return false;
}

static u64 *dm_test_get_primary_mods(struct kunit *test, struct amdgpu_device *adev)
{
	u64 *mods = NULL;
	int ret;

	ret = amdgpu_dm_plane_get_plane_modifiers(adev, DRM_PLANE_TYPE_PRIMARY, &mods);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_NOT_NULL(test, mods);

	return mods;
}

static int dm_test_gfx9_attrs(struct amdgpu_device *adev,
			      const struct amdgpu_framebuffer *afb,
				      const struct plane_size *plane_size,
				      struct dc_tiling_info *tiling_info,
				      struct dc_plane_dcc_param *dcc,
				      struct dc_plane_address *address)
{
	return amdgpu_dm_plane_fill_gfx9_plane_attributes_from_modifiers(adev,
		afb, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888, ROTATION_ANGLE_0,
		plane_size, tiling_info, dcc, address);
}

static int dm_test_gfx12_attrs(struct amdgpu_device *adev,
			       const struct amdgpu_framebuffer *afb,
				       const struct plane_size *plane_size,
				       struct dc_tiling_info *tiling_info,
				       struct dc_plane_dcc_param *dcc,
				       struct dc_plane_address *address)
{
	return amdgpu_dm_plane_fill_gfx12_plane_attributes_from_modifiers(adev,
		afb, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888, ROTATION_ANGLE_0,
		plane_size, tiling_info, dcc, address);
}

static int dm_test_plane_attrs(struct amdgpu_device *adev,
			       const struct amdgpu_framebuffer *afb,
			       enum surface_pixel_format format,
			       struct dc_tiling_info *tiling_info,
			       struct plane_size *plane_size,
			       struct dc_plane_dcc_param *dcc,
			       struct dc_plane_address *address)
{
	return amdgpu_dm_plane_fill_plane_buffer_attributes(adev, afb, format,
		ROTATION_ANGLE_0, 0, tiling_info, plane_size, dcc, address,
		false);
}

static int dm_test_video_attrs(struct amdgpu_device *adev,
			       const struct amdgpu_framebuffer *afb,
			       struct dc_tiling_info *tiling_info,
			       struct plane_size *plane_size,
			       struct dc_plane_dcc_param *dcc,
			       struct dc_plane_address *address)
{
	return dm_test_plane_attrs(adev, afb, SURFACE_PIXEL_FORMAT_VIDEO_420_YCbCr,
		tiling_info, plane_size, dcc, address);
}

static int dm_test_graphics_attrs(struct amdgpu_device *adev,
				  const struct amdgpu_framebuffer *afb,
				  struct dc_tiling_info *tiling_info,
				  struct plane_size *plane_size,
				  struct dc_plane_dcc_param *dcc,
				  struct dc_plane_address *address)
{
	return dm_test_plane_attrs(adev, afb, SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
		tiling_info, plane_size, dcc, address);
}

static void dm_test_setup_gfx9_dcc_device(struct amdgpu_device *adev,
					  struct dc *dc,
					  struct dm_test_dcc_cap_ctx *ctx,
					  bool output_independent_64b_blks)
{
	adev->family = AMDGPU_FAMILY_NV;
	adev->dm.dc = dc;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 2;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 2;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	ctx->callback_ret = true;
	ctx->capable = true;
	ctx->output_independent_64b_blks = output_independent_64b_blks;
	dm_test_dcc_ctx = ctx;
}

static u64 dm_test_gfx9_dcc_modifier(u64 tile_version, bool independent_64b_blks,
				     bool independent_128b_blks)
{
	return AMD_FMT_MOD |
	       AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
	       AMD_FMT_MOD_SET(TILE_VERSION, tile_version) |
	       AMD_FMT_MOD_SET(DCC, 1) |
	       AMD_FMT_MOD_SET(DCC_INDEPENDENT_64B, independent_64b_blks) |
	       AMD_FMT_MOD_SET(DCC_INDEPENDENT_128B, independent_128b_blks);
}

static void dm_test_setup_gfx11_device(struct amdgpu_device *adev,
				       struct dm_test_gfx11_reg_ctx *ctx,
				       u32 num_pkrs_log2, u32 num_pipes_log2)
{
	ctx->gb_addr_config =
		REG_SET_FIELD(0, GB_ADDR_CONFIG, NUM_PKRS, num_pkrs_log2) |
		REG_SET_FIELD(0, GB_ADDR_CONFIG, NUM_PIPES, num_pipes_log2);
	ctx->gc_reg_offsets[regGB_ADDR_CONFIG_BASE_IDX] = 0;
	ctx->expected_reg = ctx->gc_reg_offsets[regGB_ADDR_CONFIG_BASE_IDX] +
			    regGB_ADDR_CONFIG;
	dm_test_gfx11_reg_ctx = ctx;

	adev->family = AMDGPU_FAMILY_GC_11_0_0;
	adev->reg_offset[GC_HWIP][0] = ctx->gc_reg_offsets;
	adev->gfx.rlc.reg_funcs = &dm_test_gfx11_reg_funcs;
}

static u64 dm_test_gfx11_dcc_best_modifier(u32 pipe_xor_bits, u32 pkrs, u32 tile)
{
	return AMD_FMT_MOD |
	       AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX11) |
	       AMD_FMT_MOD_SET(PIPE_XOR_BITS, pipe_xor_bits) |
	       AMD_FMT_MOD_SET(TILE, tile) |
	       AMD_FMT_MOD_SET(PACKERS, pkrs) |
	       AMD_FMT_MOD_SET(DCC, 1) |
	       AMD_FMT_MOD_SET(DCC_INDEPENDENT_128B, 1) |
	       AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, AMD_FMT_MOD_DCC_BLOCK_128B);
}

static u64 dm_test_gfx11_dcc_4k_modifier(u32 pipe_xor_bits, u32 pkrs, u32 tile)
{
	return AMD_FMT_MOD |
	       AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX11) |
	       AMD_FMT_MOD_SET(PIPE_XOR_BITS, pipe_xor_bits) |
	       AMD_FMT_MOD_SET(TILE, tile) |
	       AMD_FMT_MOD_SET(PACKERS, pkrs) |
	       AMD_FMT_MOD_SET(DCC, 1) |
	       AMD_FMT_MOD_SET(DCC_INDEPENDENT_64B, 1) |
	       AMD_FMT_MOD_SET(DCC_INDEPENDENT_128B, 1) |
	       AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, AMD_FMT_MOD_DCC_BLOCK_64B);
}

/**
 * dm_test_get_plane_formats_overlay_universal_cap() - Verify universal overlay.
 * @test: KUnit test context.
 *
 * Verify if an overlay plane with a DCN universal plane cap reports the RGB
 * format list instead of the overlay-only list.
 */
static void dm_test_get_plane_formats_overlay_universal_cap(struct kunit *test)
{
	struct drm_plane *plane;
	struct dc_plane_cap *cap;
	u32 formats[32] = {0};

	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	cap = kunit_kzalloc(test, sizeof(*cap), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, cap);

	plane->type = DRM_PLANE_TYPE_OVERLAY;
	cap->type = DC_PLANE_TYPE_DCN_UNIVERSAL;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_get_plane_formats(plane, cap, formats, 32),
			14);
}

/**
 * dm_test_get_plane_modifiers_gfx9() - Verify GFX9 modifier list generation.
 * @test: KUnit test context.
 *
 * Verify if the GFX9 family produces a non-empty modifier list terminated by
 * LINEAR and INVALID entries.
 */
static void dm_test_get_plane_modifiers_gfx9(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_AI;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->gfx.config.gb_addr_config_fields.num_banks = 8;
	adev->gfx.config.gb_addr_config_fields.num_se = 2;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 2;

	dm_test_expect_mods_terminated(test, adev);
}

/**
 * dm_test_get_plane_modifiers_rv() - Verify RV modifier list generation.
 * @test: KUnit test context.
 *
 * Verify if pre-Raven2 RV devices add RV-specific S-swizzle modifiers and
 * non-constant-encode DCC modifiers.
 */
static void dm_test_get_plane_modifiers_rv(struct kunit *test)
{
	struct amdgpu_device *adev;
	u64 *mods;
	u64 dcc_mod;
	u64 s_x_mod;
	u64 s_mod;
	int pipes = 2;
	int pipe_xor_bits = 3;
	int bank_xor_bits = 2;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_RV;
	adev->asic_type = CHIP_RAVEN;
	adev->external_rev_id = 0x80;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->gfx.config.gb_addr_config_fields.num_banks = 4;
	adev->gfx.config.gb_addr_config_fields.num_se = 2;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 2;

	mods = dm_test_get_primary_mods(test, adev);
	dcc_mod = AMD_FMT_MOD |
		  AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
		  AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX9) |
		  AMD_FMT_MOD_SET(PIPE_XOR_BITS, pipe_xor_bits) |
		  AMD_FMT_MOD_SET(BANK_XOR_BITS, bank_xor_bits) |
		  AMD_FMT_MOD_SET(DCC, 1) |
		  AMD_FMT_MOD_SET(DCC_INDEPENDENT_64B, 1) |
		  AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, AMD_FMT_MOD_DCC_BLOCK_64B) |
		  AMD_FMT_MOD_SET(DCC_CONSTANT_ENCODE, 0);
	s_x_mod = AMD_FMT_MOD |
		  AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
		  AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX9) |
		  AMD_FMT_MOD_SET(PIPE_XOR_BITS, pipe_xor_bits) |
		  AMD_FMT_MOD_SET(BANK_XOR_BITS, bank_xor_bits);
	s_mod = AMD_FMT_MOD |
		AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S) |
		AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX9);

	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, dcc_mod));
	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, dcc_mod |
							     AMD_FMT_MOD_SET(DCC_RETILE, 1) |
							     AMD_FMT_MOD_SET(RB, 2) |
							     AMD_FMT_MOD_SET(PIPE, pipes)));
	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, s_x_mod));
	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, s_mod));

	kfree(mods);
}

/**
 * dm_test_get_plane_modifiers_rv_constant_encode() - Verify Raven2+ modifiers.
 * @test: KUnit test context.
 *
 * Verify if Raven2 and later RV devices add the constant-encode modifier
 * variants.
 */
static void dm_test_get_plane_modifiers_rv_constant_encode(struct kunit *test)
{
	struct amdgpu_device *adev;
	u64 *mods;
	u64 dcc_mod;
	int pipes = 2;
	int pipe_xor_bits = 3;
	int bank_xor_bits = 2;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_RV;
	adev->asic_type = CHIP_RAVEN;
	adev->external_rev_id = 0x81;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->gfx.config.gb_addr_config_fields.num_banks = 4;
	adev->gfx.config.gb_addr_config_fields.num_se = 2;
	adev->gfx.config.gb_addr_config_fields.num_rb_per_se = 2;

	mods = dm_test_get_primary_mods(test, adev);
	dcc_mod = AMD_FMT_MOD |
		  AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
		  AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX9) |
		  AMD_FMT_MOD_SET(PIPE_XOR_BITS, pipe_xor_bits) |
		  AMD_FMT_MOD_SET(BANK_XOR_BITS, bank_xor_bits) |
		  AMD_FMT_MOD_SET(DCC, 1) |
		  AMD_FMT_MOD_SET(DCC_INDEPENDENT_64B, 1) |
		  AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, AMD_FMT_MOD_DCC_BLOCK_64B) |
		  AMD_FMT_MOD_SET(DCC_CONSTANT_ENCODE, 1);

	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, dcc_mod));
	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, dcc_mod |
							     AMD_FMT_MOD_SET(DCC_RETILE, 1) |
							     AMD_FMT_MOD_SET(RB, 2) |
							     AMD_FMT_MOD_SET(PIPE, pipes)));

	kfree(mods);
}

/**
 * dm_test_get_plane_modifiers_gfx10_1() - Verify GFX10.1 modifier list generation.
 * @test: KUnit test context.
 *
 * Verify if a pre-10.3 NV family device dispatches to the GFX10.1 modifier
 * builder and produces a terminated list.
 */
static void dm_test_get_plane_modifiers_gfx10_1(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_NV;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 1, 0);

	dm_test_expect_mods_terminated(test, adev);
}

/**
 * dm_test_get_plane_modifiers_gfx10_3() - Verify GFX10.3 modifier list generation.
 * @test: KUnit test context.
 *
 * Verify if a 10.3+ NV family device dispatches to the GFX10.3 modifier
 * builder and produces a terminated list.
 */
static void dm_test_get_plane_modifiers_gfx10_3(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_NV;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 4;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 2;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);

	dm_test_expect_mods_terminated(test, adev);
}

/**
 * dm_test_get_plane_modifiers_gfx11_64k_first() - Verify GFX11 small-pipe order.
 * @test: KUnit test context.
 *
 * Verify if GFX11 modifier generation reads GB_ADDR_CONFIG through the RLC
 * register callback and prefers 64K_R_X when the pipe count is 16 or lower.
 */
static void dm_test_get_plane_modifiers_gfx11_64k_first(struct kunit *test)
{
	struct dm_test_gfx11_reg_ctx ctx = {0};
	struct amdgpu_device *adev;
	u64 *mods;
	u32 pipe_xor_bits = 4;
	u32 pkrs = 1;
	u32 tile = AMD_FMT_MOD_TILE_GFX9_64K_R_X;
	u64 dcc_best;
	u64 dcc_4k;
	u64 d_mod;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	dm_test_setup_gfx11_device(adev, &ctx, pkrs, pipe_xor_bits);

	mods = dm_test_get_primary_mods(test, adev);
	dcc_best = dm_test_gfx11_dcc_best_modifier(pipe_xor_bits, pkrs, tile);
	dcc_4k = dm_test_gfx11_dcc_4k_modifier(pipe_xor_bits, pkrs, tile);
	d_mod = AMD_FMT_MOD |
		AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX11) |
		AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_D);

	KUNIT_EXPECT_TRUE(test, ctx.called);
	KUNIT_EXPECT_EQ(test, ctx.captured_reg, ctx.expected_reg);
	KUNIT_EXPECT_EQ(test, ctx.captured_acc_flags, 0U);
	KUNIT_EXPECT_EQ(test, ctx.captured_hwip, (u32)GC_HWIP);
	KUNIT_EXPECT_EQ(test, ctx.captured_xcc_id, 0U);
	KUNIT_EXPECT_EQ(test, mods[0], dcc_best);
	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, dcc_4k));
	KUNIT_EXPECT_TRUE(test,
			  dm_test_mods_contain(mods,
					       dcc_best | AMD_FMT_MOD_SET(DCC_RETILE, 1)));
	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, d_mod));

	dm_test_gfx11_reg_ctx = NULL;
	kfree(mods);
}

/**
 * dm_test_get_plane_modifiers_gfx11_256k_first() - Verify GFX11 large-pipe order.
 * @test: KUnit test context.
 *
 * Verify if GFX11 modifier generation prefers 256K_R_X when more than 16 pipes
 * are reported by GB_ADDR_CONFIG.
 */
static void dm_test_get_plane_modifiers_gfx11_256k_first(struct kunit *test)
{
	struct dm_test_gfx11_reg_ctx ctx = {0};
	struct amdgpu_device *adev;
	u64 *mods;
	u32 pipe_xor_bits = 5;
	u32 pkrs = 2;
	u32 tile = AMD_FMT_MOD_TILE_GFX11_256K_R_X;
	u32 fallback_tile = AMD_FMT_MOD_TILE_GFX9_64K_R_X;
	u64 dcc_best;
	u64 fallback_dcc_best = dm_test_gfx11_dcc_best_modifier(pipe_xor_bits, pkrs,
								  fallback_tile);

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	dm_test_setup_gfx11_device(adev, &ctx, pkrs, pipe_xor_bits);

	mods = dm_test_get_primary_mods(test, adev);
	dcc_best = dm_test_gfx11_dcc_best_modifier(pipe_xor_bits, pkrs, tile);

	KUNIT_EXPECT_TRUE(test, ctx.called);
	KUNIT_EXPECT_EQ(test, ctx.captured_reg, ctx.expected_reg);
	KUNIT_EXPECT_EQ(test, mods[0], dcc_best);
	KUNIT_EXPECT_TRUE(test, dm_test_mods_contain(mods, fallback_dcc_best));

	dm_test_gfx11_reg_ctx = NULL;
	kfree(mods);
}

/**
 * dm_test_get_plane_modifiers_gfx12() - Verify GFX12 modifier list generation.
 * @test: KUnit test context.
 *
 * Verify if the GFX12 family dispatches to the GFX12 modifier builder and
 * produces a terminated list.
 */
static void dm_test_get_plane_modifiers_gfx12(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->family = AMDGPU_FAMILY_GC_12_0_0;

	dm_test_expect_mods_terminated(test, adev);
}

/**
 * dm_test_fill_gfx9_plane_attributes_dcc() - Verify GFX9 DCC modifier path.
 * @test: KUnit test context.
 *
 * Verify if a GFX10-RBPLUS DCC modifier enables DCC and selects the 64B
 * independent block mode.
 */
static void dm_test_fill_gfx9_plane_attributes_dcc(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = true,
		.capable = true,
		.output_independent_64b_blks = true,
	};
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_NV;
	adev->dm.dc = dc;
	adev->gfx.config.gb_addr_config_fields.num_pipes = 2;
	adev->gfx.config.gb_addr_config_fields.num_pkrs = 2;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	afb->base.pitches[1] = 256;
	afb->base.modifier = AMD_FMT_MOD |
			     AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_S_X) |
			     AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS) |
			     AMD_FMT_MOD_SET(DCC, 1) |
			     AMD_FMT_MOD_SET(DCC_INDEPENDENT_64B, 1);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx9_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				 &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfxversion, (int)DcGfxVersion9);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk, (int)hubp_ind_block_64b);
	KUNIT_EXPECT_EQ(test, dcc.meta_pitch, 256U);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx9_plane_attributes_validate_fails() - Verify GFX9 error path.
 * @test: KUnit test context.
 *
 * Verify if GFX9 modifier parsing returns validation errors from the shared DCC
 * validation helper.
 */
static void dm_test_fill_gfx9_plane_attributes_validate_fails(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	u64 tile_version = AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS;
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_NV;
	adev->dm.dc = dc;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);
	afb->base.modifier = dm_test_gfx9_dcc_modifier(tile_version, true, false);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx9_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				 &address);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

/**
 * dm_test_fill_gfx9_plane_attributes_dcc_rbplus_64b_no_128bcl() - Verify block mode.
 * @test: KUnit test context.
 *
 * Verify if a GFX10-RBPLUS modifier with both 64B and 128B independent block
 * bits selects the 64B-no-128BCL block mode.
 */
static void dm_test_fill_gfx9_plane_attributes_dcc_rbplus_64b_no_128bcl(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {0};
	u64 tile_version = AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS;
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	dm_test_setup_gfx9_dcc_device(adev, dc, &ctx, true);
	afb->base.modifier = dm_test_gfx9_dcc_modifier(tile_version, true, true);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx9_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				 &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk,
			(int)hubp_ind_block_64b_no_128bcl);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx9_plane_attributes_dcc_rbplus_128b() - Verify 128B block mode.
 * @test: KUnit test context.
 *
 * Verify if a GFX10-RBPLUS modifier with only the 128B independent block bit
 * selects the 128B block mode.
 */
static void dm_test_fill_gfx9_plane_attributes_dcc_rbplus_128b(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {0};
	u64 tile_version = AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS;
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	dm_test_setup_gfx9_dcc_device(adev, dc, &ctx, false);
	afb->base.modifier = dm_test_gfx9_dcc_modifier(tile_version, false, true);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx9_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				 &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk, (int)hubp_ind_block_128b);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx9_plane_attributes_dcc_rbplus_unconstrained() - Verify block mode.
 * @test: KUnit test context.
 *
 * Verify if a GFX10-RBPLUS modifier without independent block bits selects the
 * unconstrained block mode.
 */
static void dm_test_fill_gfx9_plane_attributes_dcc_rbplus_unconstrained(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {0};
	u64 tile_version = AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS;
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	dm_test_setup_gfx9_dcc_device(adev, dc, &ctx, false);
	afb->base.modifier = dm_test_gfx9_dcc_modifier(tile_version, false, false);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx9_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				 &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk,
			(int)hubp_ind_block_unconstrained);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx9_plane_attributes_dcc_gfx9_64b() - Verify legacy 64B mode.
 * @test: KUnit test context.
 *
 * Verify if a pre-RBPLUS GFX9 modifier with the 64B independent block bit
 * selects the 64B block mode.
 */
static void dm_test_fill_gfx9_plane_attributes_dcc_gfx9_64b(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {0};
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	dm_test_setup_gfx9_dcc_device(adev, dc, &ctx, true);
	afb->base.modifier = dm_test_gfx9_dcc_modifier(AMD_FMT_MOD_TILE_VER_GFX9,
						       true, false);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx9_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				 &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk, (int)hubp_ind_block_64b);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx9_plane_attributes_dcc_gfx9_unconstrained() - Verify legacy mode.
 * @test: KUnit test context.
 *
 * Verify if a pre-RBPLUS GFX9 modifier without the 64B independent block bit
 * selects the unconstrained block mode.
 */
static void dm_test_fill_gfx9_plane_attributes_dcc_gfx9_unconstrained(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {0};
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	dm_test_setup_gfx9_dcc_device(adev, dc, &ctx, false);
	afb->base.modifier = dm_test_gfx9_dcc_modifier(AMD_FMT_MOD_TILE_VER_GFX9,
						       false, false);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx9_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				 &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk,
			(int)hubp_ind_block_unconstrained);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx12_plane_attributes_block0() - Verify GFX12 64B max-compressed-block path.
 * @test: KUnit test context.
 *
 * Verify if a zero max-compressed-block modifier selects the 64B independent
 * block mode on GFX12.
 */
static void dm_test_fill_gfx12_plane_attributes_block0(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = true,
		.capable = true,
		.output_independent_64b_blks = false,
	};
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_GC_12_0_0;
	adev->dm.dc = dc;
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	afb->base.modifier = AMD_FMT_MOD |
			     AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX12_64K_2D) |
			     AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX12) |
			     AMD_FMT_MOD_SET(DCC, 1) |
			     AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, 0);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx12_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				  &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_TRUE(test, dcc.independent_64b_blks);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk, (int)hubp_ind_block_64b);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx12_plane_attributes_block_unconstrained() - Verify block path.
 * @test: KUnit test context.
 *
 * Verify if a max-compressed-block value above one selects the unconstrained
 * independent block mode on GFX12.
 */
static void dm_test_fill_gfx12_plane_attributes_block_unconstrained(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	struct dm_test_dcc_cap_ctx ctx = {
		.callback_ret = true,
		.capable = true,
		.output_independent_64b_blks = false,
	};
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_GC_12_0_0;
	adev->dm.dc = dc;
	dc->cap_funcs.get_dcc_compression_cap = dm_test_get_dcc_compression_cap;
	dm_test_dcc_ctx = &ctx;

	afb->base.modifier = AMD_FMT_MOD |
			     AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX12_64K_2D) |
			     AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX12) |
			     AMD_FMT_MOD_SET(DCC, 1) |
			     AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, 2);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx12_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				  &address);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, dcc.enable);
	KUNIT_EXPECT_FALSE(test, dcc.independent_64b_blks);
	KUNIT_EXPECT_EQ(test, (int)dcc.dcc_ind_blk,
			(int)hubp_ind_block_unconstrained);

	dm_test_dcc_ctx = NULL;
}

/**
 * dm_test_fill_gfx12_plane_attributes_validate_fails() - Verify GFX12 error path.
 * @test: KUnit test context.
 *
 * Verify if GFX12 modifier parsing returns validation errors from the shared
 * DCC validation helper.
 */
static void dm_test_fill_gfx12_plane_attributes_validate_fails(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct plane_size plane_size = {0};
	struct dc_tiling_info tiling_info = {0};
	struct dc_plane_dcc_param dcc = {0};
	struct dc_plane_address address = {0};
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_GC_12_0_0;
	adev->dm.dc = dc;
	afb->base.modifier = AMD_FMT_MOD |
			     AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX12_64K_2D) |
			     AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX12) |
			     AMD_FMT_MOD_SET(DCC, 1) |
			     AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, 1);
	plane_size.surface_size.width = 1920;
	plane_size.surface_size.height = 1080;

	ret = dm_test_gfx12_attrs(adev, afb, &plane_size, &tiling_info, &dcc,
				  &address);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

/**
 * dm_test_fill_plane_buffer_attributes_video() - Verify NV12 attributes.
 * @test: KUnit test context.
 *
 * Verify if a video pixel format fills chroma plane size and the progressive
 * video address type on a GFX9 family device.
 */
static void dm_test_fill_plane_buffer_attributes_video(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_framebuffer *afb;
	struct dc_tiling_info tiling_info;
	struct plane_size plane_size;
	struct dc_plane_dcc_param dcc;
	struct dc_plane_address address;
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	tiling_info = (struct dc_tiling_info){0};
	plane_size = (struct plane_size){0};
	dcc = (struct dc_plane_dcc_param){0};
	address = (struct dc_plane_address){0};
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_NV;
	adev->ip_versions[GC_HWIP][0] = IP_VERSION(10, 3, 0);
	afb->address = 0x80000000ULL;
	afb->base.width = 1920;
	afb->base.height = 1080;
	afb->base.offsets[0] = 0;
	afb->base.offsets[1] = 0x200000;
	afb->base.pitches[0] = 1920;
	afb->base.pitches[1] = 1920;
	afb->base.format = drm_format_info(DRM_FORMAT_NV12);
	afb->base.modifier = DRM_FORMAT_MOD_LINEAR;
	KUNIT_ASSERT_NOT_NULL(test, afb->base.format);

	ret = dm_test_video_attrs(adev, afb, &tiling_info, &plane_size, &dcc,
				  &address);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, plane_size.surface_size.width, 1920);
	KUNIT_EXPECT_EQ(test, plane_size.chroma_size.width, 960U);
	KUNIT_EXPECT_EQ(test, plane_size.chroma_size.height, 540U);
	KUNIT_EXPECT_EQ(test, address.type,
			(int)PLN_ADDR_TYPE_VIDEO_PROGRESSIVE);
	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfxversion, (int)DcGfxVersion9);
}

/**
 * dm_test_fill_plane_buffer_attributes_gfx12() - Verify GFX12 dispatch path.
 * @test: KUnit test context.
 *
 * Verify if a GFX12 family device fills graphics attributes via the GFX12
 * modifier path and reports the GFX addr3 version.
 */
static void dm_test_fill_plane_buffer_attributes_gfx12(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct amdgpu_framebuffer *afb;
	struct dc_tiling_info tiling_info;
	struct plane_size plane_size;
	struct dc_plane_dcc_param dcc;
	struct dc_plane_address address;
	int ret;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	afb = kunit_kzalloc(test, sizeof(*afb), GFP_KERNEL);
	tiling_info = (struct dc_tiling_info){0};
	plane_size = (struct plane_size){0};
	dcc = (struct dc_plane_dcc_param){0};
	address = (struct dc_plane_address){0};
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, afb);

	adev->family = AMDGPU_FAMILY_GC_12_0_0;
	adev->dm.dc = dc;
	afb->address = 0x80000000ULL;
	afb->base.width = 1920;
	afb->base.height = 1080;
	afb->base.pitches[0] = 7680;
	afb->base.format = drm_format_info(DRM_FORMAT_XRGB8888);
	afb->base.modifier = DRM_FORMAT_MOD_LINEAR;
	KUNIT_ASSERT_NOT_NULL(test, afb->base.format);

	ret = dm_test_graphics_attrs(adev, afb, &tiling_info, &plane_size, &dcc,
				     &address);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, address.type, (int)PLN_ADDR_TYPE_GRAPHICS);
	KUNIT_EXPECT_EQ(test, (int)tiling_info.gfxversion, (int)DcGfxAddr3);
}

/**
 * dm_test_get_min_max_dc_plane_scaling_fp16() - Verify fp16 cap selection.
 * @test: KUnit test context.
 *
 * Verify if 64bpp fp16 formats use the fp16 scaling caps.
 */
static void dm_test_get_min_max_dc_plane_scaling_fp16(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct drm_framebuffer *fb;
	int min_downscale = 0;
	int max_upscale = 0;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	adev->dm.dc = dc;
	dc->caps.planes[0].max_upscale_factor.fp16 = 2000;
	dc->caps.planes[0].max_downscale_factor.fp16 = 500;

	fb->format = drm_format_info(DRM_FORMAT_ARGB16161616F);
	KUNIT_ASSERT_NOT_NULL(test, fb->format);
	amdgpu_dm_plane_get_min_max_dc_plane_scaling(&adev->ddev, fb,
						     &min_downscale, &max_upscale);
	KUNIT_EXPECT_EQ(test, min_downscale, 500);
	KUNIT_EXPECT_EQ(test, max_upscale, 2000);
}

/**
 * dm_test_helper_check_state_small_viewport_width() - Verify width rejection.
 * @test: KUnit test context.
 *
 * Verify if a viewport width below the minimum pipe-split width is rejected.
 */
static void dm_test_helper_check_state_small_viewport_width(struct kunit *test)
{
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_framebuffer *fb;

	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	crtc = kunit_kzalloc(test, sizeof(*crtc), GFP_KERNEL);
	new_crtc_state = kunit_kzalloc(test, sizeof(*new_crtc_state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, crtc);
	KUNIT_ASSERT_NOT_NULL(test, new_crtc_state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	plane->type = DRM_PLANE_TYPE_OVERLAY;
	state->plane = plane;
	state->fb = fb;
	state->crtc = crtc;
	state->crtc_x = 0;
	state->crtc_y = 0;
	state->crtc_w = 10;
	state->crtc_h = 100;
	new_crtc_state->mode.crtc_hdisplay = 1920;
	new_crtc_state->mode.crtc_vdisplay = 1080;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_helper_check_state(state, new_crtc_state),
			-EINVAL);
}

/**
 * dm_test_helper_check_state_small_viewport_height() - Verify height rejection.
 * @test: KUnit test context.
 *
 * Verify if a negative-offset viewport with a too-small height is rejected.
 */
static void dm_test_helper_check_state_small_viewport_height(struct kunit *test)
{
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_framebuffer *fb;

	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	crtc = kunit_kzalloc(test, sizeof(*crtc), GFP_KERNEL);
	new_crtc_state = kunit_kzalloc(test, sizeof(*new_crtc_state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, crtc);
	KUNIT_ASSERT_NOT_NULL(test, new_crtc_state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	plane->type = DRM_PLANE_TYPE_OVERLAY;
	state->plane = plane;
	state->fb = fb;
	state->crtc = crtc;
	state->crtc_x = -2;
	state->crtc_y = -95;
	state->crtc_w = 100;
	state->crtc_h = 100;
	new_crtc_state->mode.crtc_hdisplay = 1920;
	new_crtc_state->mode.crtc_vdisplay = 1080;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_helper_check_state(state, new_crtc_state),
			-EINVAL);
}

/**
 * dm_test_helper_check_state_bottom_clipped_height() - Verify bottom clipping.
 * @test: KUnit test context.
 *
 * Verify if a viewport clipped by the bottom edge to below the minimum height
 * is rejected.
 */
static void dm_test_helper_check_state_bottom_clipped_height(struct kunit *test)
{
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_framebuffer *fb;

	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	crtc = kunit_kzalloc(test, sizeof(*crtc), GFP_KERNEL);
	new_crtc_state = kunit_kzalloc(test, sizeof(*new_crtc_state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, crtc);
	KUNIT_ASSERT_NOT_NULL(test, new_crtc_state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	plane->type = DRM_PLANE_TYPE_OVERLAY;
	state->plane = plane;
	state->fb = fb;
	state->crtc = crtc;
	state->crtc_x = 0;
	state->crtc_y = 95;
	state->crtc_w = 100;
	state->crtc_h = 100;
	new_crtc_state->mode.crtc_hdisplay = 1920;
	new_crtc_state->mode.crtc_vdisplay = 100;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_helper_check_state(state, new_crtc_state),
			-EINVAL);
}

/**
 * dm_test_helper_check_state_scaling_caps() - Verify DC scaling caps are applied.
 * @test: KUnit test context.
 *
 * Verify if helper_check_state converts DC plane scaling caps to DRM scale
 * limits and rejects scaling outside those limits.
 */
static void dm_test_helper_check_state_scaling_caps(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_framebuffer *fb;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	crtc = kunit_kzalloc(test, sizeof(*crtc), GFP_KERNEL);
	new_crtc_state = kunit_kzalloc(test, sizeof(*new_crtc_state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, crtc);
	KUNIT_ASSERT_NOT_NULL(test, new_crtc_state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	adev->dm.dc = dc;
	dc->caps.planes[0].max_upscale_factor.argb8888 = 1;
	dc->caps.planes[0].max_downscale_factor.argb8888 = 1;

	plane->type = DRM_PLANE_TYPE_OVERLAY;
	plane->dev = &adev->ddev;
	crtc->dev = &adev->ddev;
	fb->width = 100;
	fb->height = 100;
	fb->format = drm_format_info(DRM_FORMAT_XRGB8888);
	KUNIT_ASSERT_NOT_NULL(test, fb->format);

	state->plane = plane;
	state->fb = fb;
	state->crtc = crtc;
	state->src_w = 100 << 16;
	state->src_h = 100 << 16;
	state->crtc_w = 200;
	state->crtc_h = 200;
	new_crtc_state->crtc = crtc;
	new_crtc_state->mode.crtc_hdisplay = 1920;
	new_crtc_state->mode.crtc_vdisplay = 1080;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_helper_check_state(state, new_crtc_state),
			-ERANGE);
}

/**
 * dm_test_fill_dc_scaling_info_nv12_dcn1x() - Verify NV12 DCN1x rejection.
 * @test: KUnit test context.
 *
 * Verify if a non-zero NV12 source origin is rejected on DCN 1.0 to avoid the
 * known DCN1x hang.
 */
static void dm_test_fill_dc_scaling_info_nv12_dcn1x(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct drm_plane_state state = {0};
	struct drm_framebuffer fb = {0};
	struct dc_scaling_info info = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	adev->ip_versions[DCE_HWIP][0] = IP_VERSION(1, 0, 0);
	fb.format = drm_format_info(DRM_FORMAT_NV12);
	KUNIT_ASSERT_NOT_NULL(test, fb.format);

	state.fb = &fb;
	state.src_x = 10 << 16;
	state.src_y = 0;
	state.src_w = 100 << 16;
	state.src_h = 100 << 16;
	state.crtc_w = 100;
	state.crtc_h = 100;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_fill_dc_scaling_info(adev, &state, &info),
			-EINVAL);

	state.src_x = 0;
	state.src_y = 10 << 16;
	memset(&info, 0, sizeof(info));

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_fill_dc_scaling_info(adev, &state, &info),
			-EINVAL);
}

/**
 * dm_test_fill_dc_scaling_info_plane_caps() - Verify scaling caps path.
 * @test: KUnit test context.
 *
 * Verify if scaling info uses plane caps when the state references a plane,
 * device, and framebuffer.
 */
static void dm_test_fill_dc_scaling_info_plane_caps(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc *dc;
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_framebuffer *fb;
	struct dc_scaling_info info = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	adev->dm.dc = dc;
	dc->caps.planes[0].max_upscale_factor.argb8888 = 16000;
	dc->caps.planes[0].max_downscale_factor.argb8888 = 250;

	plane->dev = &adev->ddev;
	fb->format = drm_format_info(DRM_FORMAT_XRGB8888);
	KUNIT_ASSERT_NOT_NULL(test, fb->format);

	state->plane = plane;
	state->fb = fb;
	state->src_w = 100 << 16;
	state->src_h = 100 << 16;
	state->crtc_w = 100;
	state->crtc_h = 100;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_fill_dc_scaling_info(adev, state, &info),
			0);
}

/**
 * dm_test_get_cursor_position_bad_size() - Verify oversized cursor rejection.
 * @test: KUnit test context.
 *
 * Verify if a cursor larger than the CRTC maximum is rejected.
 */
static void dm_test_get_cursor_position_bad_size(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *amdgpu_crtc;
	struct drm_plane *plane;
	struct drm_plane_state *state;
	struct drm_framebuffer *fb;
	struct dc_cursor_position position = {0};

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	amdgpu_crtc = kunit_kzalloc(test, sizeof(*amdgpu_crtc), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	state = kunit_kzalloc(test, sizeof(*state), GFP_KERNEL);
	fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, amdgpu_crtc);
	KUNIT_ASSERT_NOT_NULL(test, plane);
	KUNIT_ASSERT_NOT_NULL(test, state);
	KUNIT_ASSERT_NOT_NULL(test, fb);

	amdgpu_crtc->max_cursor_width = 64;
	amdgpu_crtc->max_cursor_height = 64;
	plane->dev = &adev->ddev;
	plane->state = state;
	state->fb = fb;
	state->crtc_w = 128;
	state->crtc_h = 32;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_plane_get_cursor_position(plane, &amdgpu_crtc->base, &position),
			-EINVAL);
}

/**
 * dm_test_format_mod_supported_d_swizzle_reject() - Verify D swizzle rejection.
 * @test: KUnit test context.
 *
 * Verify if a D micro-swizzle modifier is rejected for formats narrower than
 * 8 bytes per pixel.
 */
static void dm_test_format_mod_supported_d_swizzle_reject(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct drm_plane *plane;
	u64 listed_mod;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	plane = kunit_kzalloc(test, sizeof(*plane), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	KUNIT_ASSERT_NOT_NULL(test, plane);

	adev->family = AMDGPU_FAMILY_RV;
	plane->dev = &adev->ddev;

	listed_mod = AMD_FMT_MOD |
		     AMD_FMT_MOD_SET(TILE, AMD_FMT_MOD_TILE_GFX9_64K_D) |
		     AMD_FMT_MOD_SET(TILE_VERSION, AMD_FMT_MOD_TILE_VER_GFX9);
	plane->modifiers = &listed_mod;
	plane->modifier_count = 1;

	KUNIT_EXPECT_FALSE(test,
			   amdgpu_dm_plane_format_mod_supported(plane, DRM_FORMAT_XRGB8888,
								listed_mod));
}

static struct kunit_case amdgpu_dm_plane_test_cases[] = {
	/* amdgpu_dm_plane_is_video_format() */
	KUNIT_CASE(dm_test_plane_is_video_format_known_video),
	/* amdgpu_dm_plane_get_format_info() */
	KUNIT_CASE(dm_test_get_format_info),
	/* amdgpu_dm_plane_fill_blending_from_plane_state() */
	KUNIT_CASE(dm_test_fill_blending_defaults),
	KUNIT_CASE(dm_test_fill_blending_premulti_alpha_format),
	KUNIT_CASE(dm_test_fill_blending_coverage_alpha_format),
	KUNIT_CASE(dm_test_fill_blending_global_alpha),
	KUNIT_CASE(dm_test_fill_blending_global_alpha_dcn42),
	/* amdgpu_dm_plane_modifier_* helpers() */
	KUNIT_CASE(dm_test_modifier_has_dcc),
	KUNIT_CASE(dm_test_modifier_gfx9_swizzle_mode),
	/* amdgpu_dm_plane_get_plane_formats() */
	KUNIT_CASE(dm_test_get_plane_formats),
	KUNIT_CASE(dm_test_get_plane_formats_overlay_universal_cap),
	/* amdgpu_dm_plane_get_plane_modifiers() */
	KUNIT_CASE(dm_test_get_plane_modifiers),
	KUNIT_CASE(dm_test_get_plane_modifiers_gfx9),
	KUNIT_CASE(dm_test_get_plane_modifiers_rv),
	KUNIT_CASE(dm_test_get_plane_modifiers_rv_constant_encode),
	KUNIT_CASE(dm_test_get_plane_modifiers_gfx10_1),
	KUNIT_CASE(dm_test_get_plane_modifiers_gfx10_3),
	KUNIT_CASE(dm_test_get_plane_modifiers_gfx11_64k_first),
	KUNIT_CASE(dm_test_get_plane_modifiers_gfx11_256k_first),
	KUNIT_CASE(dm_test_get_plane_modifiers_gfx12),
	/* amdgpu_dm_plane_fill_dc_scaling_info() */
	KUNIT_CASE(dm_test_fill_dc_scaling_info),
	KUNIT_CASE(dm_test_fill_dc_scaling_info_nv12_dcn1x),
	KUNIT_CASE(dm_test_fill_dc_scaling_info_plane_caps),
	/* amdgpu_dm_plane_get_min_max_dc_plane_scaling() */
	KUNIT_CASE(dm_test_get_min_max_dc_plane_scaling),
	KUNIT_CASE(dm_test_get_min_max_dc_plane_scaling_fp16),
	/* amdgpu_dm_plane_fill_plane_buffer_attributes() */
	KUNIT_CASE(dm_test_fill_plane_buffer_attributes_gfx8),
	KUNIT_CASE(dm_test_fill_plane_buffer_attributes_video),
	KUNIT_CASE(dm_test_fill_plane_buffer_attributes_gfx12),
	/* amdgpu_dm_plane_get_cursor_position() */
	KUNIT_CASE(dm_test_get_cursor_position),
	KUNIT_CASE(dm_test_get_cursor_position_bad_size),
	/* amdgpu_dm_plane_format_mod_supported() */
	KUNIT_CASE(dm_test_format_mod_supported),
	KUNIT_CASE(dm_test_format_mod_supported_d_swizzle_reject),
	/* amdgpu_dm_plane_fill_gfx12_plane_attributes_from_modifiers() */
	KUNIT_CASE(dm_test_fill_gfx12_plane_attributes_from_modifiers),
	KUNIT_CASE(dm_test_fill_gfx12_plane_attributes_block0),
	KUNIT_CASE(dm_test_fill_gfx12_plane_attributes_block_unconstrained),
	KUNIT_CASE(dm_test_fill_gfx12_plane_attributes_validate_fails),
	/* amdgpu_dm_plane_fill_gfx9_plane_attributes_from_modifiers() */
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_from_modifiers),
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_dcc),
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_validate_fails),
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_dcc_rbplus_64b_no_128bcl),
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_dcc_rbplus_128b),
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_dcc_rbplus_unconstrained),
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_dcc_gfx9_64b),
	KUNIT_CASE(dm_test_fill_gfx9_plane_attributes_dcc_gfx9_unconstrained),
	/* amdgpu_dm_plane_helper_check_state() */
	KUNIT_CASE(dm_test_helper_check_state_viewport_reject),
	KUNIT_CASE(dm_test_helper_check_state_small_viewport_width),
	KUNIT_CASE(dm_test_helper_check_state_small_viewport_height),
	KUNIT_CASE(dm_test_helper_check_state_bottom_clipped_height),
	KUNIT_CASE(dm_test_helper_check_state_scaling_caps),
	/* amdgpu_dm_plane_add_modifier() */
	KUNIT_CASE(dm_test_add_modifier_appends_value),
	KUNIT_CASE(dm_test_add_modifier_grows_capacity),
	KUNIT_CASE(dm_test_add_modifier_noop_when_mods_null),
	/* amdgpu_dm_plane_fill_gfx8_tiling_info_from_flags() */
	KUNIT_CASE(dm_test_fill_gfx8_tiling_info_2d_tiled),
	KUNIT_CASE(dm_test_fill_gfx8_tiling_info_1d_tiled),
	KUNIT_CASE(dm_test_fill_gfx8_tiling_info_other_mode),
	/* amdgpu_dm_plane_fill_gfx9_tiling_info_from_device() */
	KUNIT_CASE(dm_test_fill_gfx9_tiling_info_from_device_pre_10_3),
	KUNIT_CASE(dm_test_fill_gfx9_tiling_info_from_device_10_3_plus),
	/* amdgpu_dm_plane_fill_gfx9_tiling_info_from_modifier() */
	KUNIT_CASE(dm_test_fill_gfx9_tiling_info_from_modifier_linear),
	KUNIT_CASE(dm_test_fill_gfx9_tiling_info_from_modifier_pre_nv),
	KUNIT_CASE(dm_test_fill_gfx9_tiling_info_from_modifier_nv),
	/* amdgpu_dm_plane_validate_dcc() */
	KUNIT_CASE(dm_test_validate_dcc_disabled_returns_success),
	KUNIT_CASE(dm_test_validate_dcc_video_non_gfx12_fails),
	KUNIT_CASE(dm_test_validate_dcc_missing_cap_func_fails),
	KUNIT_CASE(dm_test_validate_dcc_cap_callback_fails),
	KUNIT_CASE(dm_test_validate_dcc_not_capable_fails),
	KUNIT_CASE(dm_test_validate_dcc_success_and_scan_mapping),
	KUNIT_CASE(dm_test_validate_dcc_independent_64b_mismatch_fails),
	{}
};

static struct kunit_suite amdgpu_dm_plane_test_suite = {
	.name = "amdgpu_dm_plane",
	.test_cases = amdgpu_dm_plane_test_cases,
};

kunit_test_suite(amdgpu_dm_plane_test_suite);

MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_plane");
MODULE_LICENSE("Dual MIT/GPL");
