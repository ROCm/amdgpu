// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_helpers.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <drm/drm_edid.h>
#include <drm/drm_kunit_helpers.h>

#include "dc.h"
#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "dm_helpers.h"
#include "ddc_service_types.h"
#include "amdgpu_dm_helpers.h"

/* Tests for edid_extract_panel_id() */

/**
 * dm_test_edid_extract_panel_id_basic - Test Edid extract panel id basic
 * @test: The KUnit test context
 */
static void dm_test_edid_extract_panel_id_basic(struct kunit *test)
{
	struct edid *edid;
	u32 panel_id;

	edid = kunit_kzalloc(test, sizeof(*edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid);

	edid->mfg_id[0] = 0x12;
	edid->mfg_id[1] = 0x34;
	edid->prod_code[0] = 0xAB;
	edid->prod_code[1] = 0xCD;

	panel_id = edid_extract_panel_id(edid);

	/*
	 * Expected: (0x12 << 24) | (0x34 << 16) | EDID_PRODUCT_ID(edid)
	 * EDID_PRODUCT_ID = prod_code[0] | (prod_code[1] << 8) = 0xAB | 0xCD00 = 0xCDAB
	 * Result: 0x12340000 | 0x0000CDAB = 0x1234CDAB
	 */
	KUNIT_EXPECT_EQ(test, panel_id, (u32)0x1234CDAB);
}

/**
 * dm_test_edid_extract_panel_id_zeros - Test Edid extract panel id zeros
 * @test: The KUnit test context
 */
static void dm_test_edid_extract_panel_id_zeros(struct kunit *test)
{
	struct edid *edid;

	edid = kunit_kzalloc(test, sizeof(*edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid);

	KUNIT_EXPECT_EQ(test, edid_extract_panel_id(edid), 0U);
}

/* Tests for dm_is_freesync_pcon_whitelist() */

/**
 * dm_test_freesync_pcon_whitelist_all_known - Test all known Freesync Pcon whitelist entries
 * @test: The KUnit test context
 *
 * Iterates over the driver's whitelist table directly so that any ID added
 * to dm_freesync_pcon_whitelist[] is automatically covered by this test.
 */
static void dm_test_freesync_pcon_whitelist_all_known(struct kunit *test)
{
	u32 i;

	for (i = 0; i < dm_freesync_pcon_whitelist_count(); i++)
		KUNIT_EXPECT_TRUE(test,
				  dm_is_freesync_pcon_whitelist(dm_freesync_pcon_whitelist[i]));
}

/**
 * dm_test_freesync_pcon_whitelist_not_in_list - Test Freesync pcon whitelist not in list
 * @test: The KUnit test context
 */
static void dm_test_freesync_pcon_whitelist_not_in_list(struct kunit *test)
{
	/* 0xFFFFFF is not a known whitelist device */
	KUNIT_EXPECT_FALSE(test, dm_is_freesync_pcon_whitelist(0xFFFFFF));
}

/**
 * dm_test_freesync_pcon_whitelist_zero - Test Freesync pcon whitelist zero
 * @test: The KUnit test context
 */
static void dm_test_freesync_pcon_whitelist_zero(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, dm_is_freesync_pcon_whitelist(0));
}

/* Tests for populate_hdmi_info_from_connector() */

/**
 * dm_test_populate_hdmi_scdc_present_true - Test Populate hdmi scdc present true
 * @test: The KUnit test context
 */
static void dm_test_populate_hdmi_scdc_present_true(struct kunit *test)
{
	struct drm_hdmi_info *hdmi;
	struct dc_edid_caps *caps;

	hdmi = kunit_kzalloc(test, sizeof(*hdmi), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, hdmi);
	caps = kunit_kzalloc(test, sizeof(*caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, caps);

	hdmi->scdc.supported = true;

	populate_hdmi_info_from_connector(true, hdmi, caps);

	KUNIT_EXPECT_TRUE(test, caps->scdc_present);
}

/**
 * dm_test_populate_hdmi_scdc_present_false - Test Populate hdmi scdc present false
 * @test: The KUnit test context
 */
static void dm_test_populate_hdmi_scdc_present_false(struct kunit *test)
{
	struct drm_hdmi_info *hdmi;
	struct dc_edid_caps *caps;

	hdmi = kunit_kzalloc(test, sizeof(*hdmi), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, hdmi);
	caps = kunit_kzalloc(test, sizeof(*caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, caps);

	hdmi->scdc.supported = false;
	caps->scdc_present = true; /* pre-set to confirm it gets cleared */

	populate_hdmi_info_from_connector(true, hdmi, caps);

	KUNIT_EXPECT_FALSE(test, caps->scdc_present);
}

/**
 * dm_test_populate_hdmi_frl_dsc_10bpc - Test HDMI FRL DSC 10 bpc caps
 * @test: The KUnit test context
 */
static void dm_test_populate_hdmi_frl_dsc_10bpc(struct kunit *test)
{
	struct drm_hdmi_info *hdmi;
	struct dc_edid_caps *caps;

	hdmi = kunit_kzalloc(test, sizeof(*hdmi), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, hdmi);
	caps = kunit_kzalloc(test, sizeof(*caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, caps);

	hdmi->max_lanes = 4;
	hdmi->max_frl_rate_per_lane = 12;
	hdmi->dsc_cap.v_1p2 = true;
	hdmi->dsc_cap.bpc_supported = 10;
	hdmi->dsc_cap.all_bpp = true;
	hdmi->dsc_cap.native_420 = true;
	hdmi->dsc_cap.max_slices = 8;
	hdmi->dsc_cap.clk_per_slice = 400;
	hdmi->dsc_cap.max_lanes = 4;
	hdmi->dsc_cap.max_frl_rate_per_lane = 10;
	hdmi->dsc_cap.total_chunk_kbytes = 7;

	populate_hdmi_info_from_connector(true, hdmi, caps);

	KUNIT_EXPECT_EQ(test, caps->max_frl_rate, 6);
	KUNIT_EXPECT_TRUE(test, caps->frl_dsc_support);
	KUNIT_EXPECT_TRUE(test, caps->frl_dsc_10bpc);
	KUNIT_EXPECT_FALSE(test, caps->frl_dsc_12bpc);
	KUNIT_EXPECT_TRUE(test, caps->frl_dsc_all_bpp);
	KUNIT_EXPECT_TRUE(test, caps->frl_dsc_native_420);
	KUNIT_EXPECT_EQ(test, caps->frl_dsc_max_slices, 5);
	KUNIT_EXPECT_EQ(test, caps->frl_dsc_max_frl_rate, 5);
	KUNIT_EXPECT_EQ(test, caps->frl_dsc_total_chunk_kbytes, 7);
}

/**
 * dm_test_populate_hdmi_frl_dsc_12bpc - Test HDMI FRL DSC 12 bpc caps
 * @test: The KUnit test context
 */
static void dm_test_populate_hdmi_frl_dsc_12bpc(struct kunit *test)
{
	struct drm_hdmi_info *hdmi;
	struct dc_edid_caps *caps;

	hdmi = kunit_kzalloc(test, sizeof(*hdmi), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, hdmi);
	caps = kunit_kzalloc(test, sizeof(*caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, caps);

	hdmi->max_lanes = 3;
	hdmi->max_frl_rate_per_lane = 6;
	hdmi->dsc_cap.v_1p2 = true;
	hdmi->dsc_cap.bpc_supported = 12;
	hdmi->dsc_cap.max_slices = 16;
	hdmi->dsc_cap.clk_per_slice = 400;
	hdmi->dsc_cap.max_lanes = 3;
	hdmi->dsc_cap.max_frl_rate_per_lane = 3;

	populate_hdmi_info_from_connector(true, hdmi, caps);

	KUNIT_EXPECT_EQ(test, caps->max_frl_rate, 2);
	KUNIT_EXPECT_TRUE(test, caps->frl_dsc_support);
	KUNIT_EXPECT_FALSE(test, caps->frl_dsc_10bpc);
	KUNIT_EXPECT_TRUE(test, caps->frl_dsc_12bpc);
	KUNIT_EXPECT_EQ(test, caps->frl_dsc_max_slices, 7);
	KUNIT_EXPECT_EQ(test, caps->frl_dsc_max_frl_rate, 1);
}

/**
 * dm_test_populate_hdmi_frl_dsc_unknown_values - Test HDMI FRL DSC unknown values
 * @test: The KUnit test context
 */
static void dm_test_populate_hdmi_frl_dsc_unknown_values(struct kunit *test)
{
	struct drm_hdmi_info *hdmi;
	struct dc_edid_caps *caps;

	hdmi = kunit_kzalloc(test, sizeof(*hdmi), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, hdmi);
	caps = kunit_kzalloc(test, sizeof(*caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, caps);

	hdmi->max_lanes = 2;
	hdmi->max_frl_rate_per_lane = 3;
	hdmi->dsc_cap.v_1p2 = true;
	hdmi->dsc_cap.bpc_supported = 8;
	hdmi->dsc_cap.max_slices = 3;
	hdmi->dsc_cap.clk_per_slice = 340;
	hdmi->dsc_cap.max_lanes = 2;
	hdmi->dsc_cap.max_frl_rate_per_lane = 12;

	populate_hdmi_info_from_connector(true, hdmi, caps);

	KUNIT_EXPECT_EQ(test, caps->max_frl_rate, 0);
	KUNIT_EXPECT_TRUE(test, caps->frl_dsc_support);
	KUNIT_EXPECT_FALSE(test, caps->frl_dsc_10bpc);
	KUNIT_EXPECT_FALSE(test, caps->frl_dsc_12bpc);
	KUNIT_EXPECT_EQ(test, caps->frl_dsc_max_slices, 0);
	KUNIT_EXPECT_EQ(test, caps->frl_dsc_max_frl_rate, 0);
}

/* Tests for dm_get_adaptive_sync_support_type() */

/**
 * dm_test_adaptive_sync_type_none_default - Test Adaptive sync type none default
 * @test: The KUnit test context
 */
static void dm_test_adaptive_sync_type_none_default(struct kunit *test)
{
	struct dc_link *link;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* dongle_type = 0 (DISPLAY_DONGLE_NONE) → default case → TYPE_NONE */
	KUNIT_EXPECT_EQ(test,
			(int)dm_get_adaptive_sync_support_type(link),
			(int)ADAPTIVE_SYNC_TYPE_NONE);
}

/**
 * dm_test_adaptive_sync_type_converter_no_conditions - Converter without caps
 * @test: The KUnit test context
 */
static void dm_test_adaptive_sync_type_converter_no_conditions(struct kunit *test)
{
	struct dc_link *link;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* HDMI converter but no adaptive sync cap → still NONE */
	link->dpcd_caps.dongle_type = DISPLAY_DONGLE_DP_HDMI_CONVERTER;

	KUNIT_EXPECT_EQ(test,
			(int)dm_get_adaptive_sync_support_type(link),
			(int)ADAPTIVE_SYNC_TYPE_NONE);
}

/**
 * dm_test_adaptive_sync_type_converter_partial_conditions - Partial caps
 * @test: The KUnit test context
 */
static void dm_test_adaptive_sync_type_converter_partial_conditions(struct kunit *test)
{
	struct dc_link *link;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* Cap set and whitelist ID, but allow_invalid_MSA_timing_param = false */
	link->dpcd_caps.dongle_type = DISPLAY_DONGLE_DP_HDMI_CONVERTER;
	link->dpcd_caps.adaptive_sync_caps.dp_adap_sync_caps.bits.ADAPTIVE_SYNC_SDP_SUPPORT = 1;
	link->dpcd_caps.allow_invalid_MSA_timing_param = false;
	link->dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_0060AD;

	KUNIT_EXPECT_EQ(test,
			(int)dm_get_adaptive_sync_support_type(link),
			(int)ADAPTIVE_SYNC_TYPE_NONE);
}

/**
 * dm_test_adaptive_sync_type_pcon_whitelist - Test Adaptive sync type pcon whitelist
 * @test: The KUnit test context
 */
static void dm_test_adaptive_sync_type_pcon_whitelist(struct kunit *test)
{
	struct dc_link *link;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* All conditions met → FREESYNC_TYPE_PCON_IN_WHITELIST */
	link->dpcd_caps.dongle_type = DISPLAY_DONGLE_DP_HDMI_CONVERTER;
	link->dpcd_caps.adaptive_sync_caps.dp_adap_sync_caps.bits.ADAPTIVE_SYNC_SDP_SUPPORT = 1;
	link->dpcd_caps.allow_invalid_MSA_timing_param = true;
	link->dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_0060AD;

	KUNIT_EXPECT_EQ(test,
			(int)dm_get_adaptive_sync_support_type(link),
			(int)FREESYNC_TYPE_PCON_IN_WHITELIST);
}

/**
 * dm_test_adaptive_sync_type_converter_nonwhitelist - Converter not whitelisted
 * @test: The KUnit test context
 */
static void dm_test_adaptive_sync_type_converter_nonwhitelist(struct kunit *test)
{
	struct dc_link *link;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* All conditions met but branch_dev_id not in whitelist → NONE */
	link->dpcd_caps.dongle_type = DISPLAY_DONGLE_DP_HDMI_CONVERTER;
	link->dpcd_caps.adaptive_sync_caps.dp_adap_sync_caps.bits.ADAPTIVE_SYNC_SDP_SUPPORT = 1;
	link->dpcd_caps.allow_invalid_MSA_timing_param = true;
	link->dpcd_caps.branch_dev_id = 0xFFFFFF;

	KUNIT_EXPECT_EQ(test,
			(int)dm_get_adaptive_sync_support_type(link),
			(int)ADAPTIVE_SYNC_TYPE_NONE);
}

/* Tests for dm_helpers_is_fullscreen() and dm_helpers_is_hdr_on() */

/**
 * dm_test_helpers_is_fullscreen_returns_false - Test Helpers is fullscreen returns false
 * @test: The KUnit test context
 */
static void dm_test_helpers_is_fullscreen_returns_false(struct kunit *test)
{
	/* Stub — always returns false */
	KUNIT_EXPECT_FALSE(test, dm_helpers_is_fullscreen(NULL, NULL));
}

/**
 * dm_test_helpers_is_hdr_on_returns_false - Test Helpers is hdr on returns false
 * @test: The KUnit test context
 */
static void dm_test_helpers_is_hdr_on_returns_false(struct kunit *test)
{
	/* Stub — always returns false */
	KUNIT_EXPECT_FALSE(test, dm_helpers_is_hdr_on(NULL, NULL));
}

/* Tests for get_max_frl_rate() */

/**
 * dm_test_get_max_frl_rate_3lanes_3gbps - Test Get max frl rate 3lanes 3gbps
 * @test: The KUnit test context
 */
static void dm_test_get_max_frl_rate_3lanes_3gbps(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_max_frl_rate(3, 3), 1);
}

/**
 * dm_test_get_max_frl_rate_3lanes_6gbps - Test Get max frl rate 3lanes 6gbps
 * @test: The KUnit test context
 */
static void dm_test_get_max_frl_rate_3lanes_6gbps(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_max_frl_rate(3, 6), 2);
}

/**
 * dm_test_get_max_frl_rate_4lanes_6gbps - Test Get max frl rate 4lanes 6gbps
 * @test: The KUnit test context
 */
static void dm_test_get_max_frl_rate_4lanes_6gbps(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_max_frl_rate(4, 6), 3);
}

/**
 * dm_test_get_max_frl_rate_4lanes_8gbps - Test Get max frl rate 4lanes 8gbps
 * @test: The KUnit test context
 */
static void dm_test_get_max_frl_rate_4lanes_8gbps(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_max_frl_rate(4, 8), 4);
}

/**
 * dm_test_get_max_frl_rate_4lanes_10gbps - Test Get max frl rate 4lanes 10gbps
 * @test: The KUnit test context
 */
static void dm_test_get_max_frl_rate_4lanes_10gbps(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_max_frl_rate(4, 10), 5);
}

/**
 * dm_test_get_max_frl_rate_4lanes_12gbps - Test Get max frl rate 4lanes 12gbps
 * @test: The KUnit test context
 */
static void dm_test_get_max_frl_rate_4lanes_12gbps(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_max_frl_rate(4, 12), 6);
}

/**
 * dm_test_get_max_frl_rate_unknown - Test Get max frl rate unknown
 * @test: The KUnit test context
 */
static void dm_test_get_max_frl_rate_unknown(struct kunit *test)
{
	/* Unknown lane/rate combination → 0 */
	KUNIT_EXPECT_EQ(test, get_max_frl_rate(2, 3), 0);
}

/* Tests for dm_dtn_log_begin() / dm_dtn_log_append_v() / dm_dtn_log_end() */

/**
 * dm_test_dtn_log_buffer_accumulates - Test DTN log buffer accumulation
 * @test: The KUnit test context
 */
static void dm_test_dtn_log_buffer_accumulates(struct kunit *test)
{
	struct dc_log_buffer_ctx log_ctx = {0};

	dm_dtn_log_begin(NULL, &log_ctx);
	dm_dtn_log_append_v(NULL, &log_ctx, "x=%d\n", 7);
	dm_dtn_log_end(NULL, &log_ctx);

	KUNIT_ASSERT_NOT_NULL(test, log_ctx.buf);
	KUNIT_EXPECT_STREQ(test, log_ctx.buf, "[dtn begin]\nx=7\n[dtn end]\n");
	KUNIT_EXPECT_EQ(test, log_ctx.pos, strlen("[dtn begin]\nx=7\n[dtn end]\n"));

	kvfree(log_ctx.buf);
}

/**
 * dm_test_dtn_log_null_ctx_no_crash - Test DTN log helpers with NULL log buffer
 * @test: The KUnit test context
 */
static void dm_test_dtn_log_null_ctx_no_crash(struct kunit *test)
{
	/* NULL log_ctx redirects to dmesg and must not dereference a buffer */
	dm_dtn_log_begin(NULL, NULL);
	dm_dtn_log_append_v(NULL, NULL, "value %d\n", 1);
	dm_dtn_log_end(NULL, NULL);

	KUNIT_EXPECT_TRUE(test, true);
}

/* Tests for dm_helpers_dp_read_dpcd() / dm_helpers_dp_write_dpcd() */

/**
 * dm_test_dp_read_dpcd_null_priv - Test DPCD read returns false without connector
 * @test: The KUnit test context
 */
static void dm_test_dp_read_dpcd_null_priv(struct kunit *test)
{
	struct dc_link *link;
	uint8_t data = 0;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* link->priv (aconnector) is NULL → early return false */
	KUNIT_EXPECT_FALSE(test,
			   dm_helpers_dp_read_dpcd(NULL, link, 0, &data, sizeof(data)));
}

/**
 * dm_test_dp_write_dpcd_null_priv - Test DPCD write returns false without connector
 * @test: The KUnit test context
 */
static void dm_test_dp_write_dpcd_null_priv(struct kunit *test)
{
	struct dc_link *link;
	uint8_t data = 0;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* link->priv (aconnector) is NULL → early return false */
	KUNIT_EXPECT_FALSE(test,
			   dm_helpers_dp_write_dpcd(NULL, link, 0, &data, sizeof(data)));
}

/* Tests for dm_helpers_dp_mst_start_top_mgr() / dm_helpers_dp_mst_stop_top_mgr() */

/**
 * dm_test_mst_start_top_mgr_null_priv - Test MST start returns false without connector
 * @test: The KUnit test context
 */
static void dm_test_mst_start_top_mgr_null_priv(struct kunit *test)
{
	struct dc_link *link;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	KUNIT_EXPECT_FALSE(test, dm_helpers_dp_mst_start_top_mgr(NULL, link, false));
}

/**
 * dm_test_mst_stop_top_mgr_null_priv - Test MST stop returns false without connector
 * @test: The KUnit test context
 */
static void dm_test_mst_stop_top_mgr_null_priv(struct kunit *test)
{
	struct dc_link *link;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	KUNIT_EXPECT_FALSE(test, dm_helpers_dp_mst_stop_top_mgr(NULL, link));
}

/**
 * dm_test_mst_start_top_mgr_boot - Test MST start boot path on a connector-backed link
 * @test: The KUnit test context
 *
 * Uses the DRM KUnit mock device to back the connector so the link is a
 * realistic connector-backed link. The boot path short-circuits and returns
 * true without touching the MST topology manager.
 */
static void dm_test_mst_start_top_mgr_boot(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;
	struct drm_device *drm;
	struct device *dev;
	struct dc_link *link;

	dev = drm_kunit_helper_alloc_device(test);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	drm = __drm_kunit_helper_alloc_drm_device(test, dev,
						  sizeof(*adev),
						  offsetof(struct amdgpu_device, ddev),
						  DRIVER_MODESET);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drm);
	adev = drm_to_adev(drm);

	aconnector = kunit_kzalloc(test, sizeof(*aconnector), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, aconnector);
	aconnector->base.dev = drm;

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);
	link->priv = aconnector;

	KUNIT_EXPECT_TRUE(test, dm_helpers_dp_mst_start_top_mgr(NULL, link, true));
}

/* Tests for dm_helpers_dp_write_hblank_reduction() */

/**
 * dm_test_dp_write_hblank_reduction_false - Test hblank reduction stub returns false
 * @test: The KUnit test context
 */
static void dm_test_dp_write_hblank_reduction_false(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, dm_helpers_dp_write_hblank_reduction(NULL, NULL));
}

static struct kunit_case amdgpu_dm_helpers_test_cases[] = {
	/* edid_extract_panel_id */
	KUNIT_CASE(dm_test_edid_extract_panel_id_basic),
	KUNIT_CASE(dm_test_edid_extract_panel_id_zeros),
	/* dm_is_freesync_pcon_whitelist */
	KUNIT_CASE(dm_test_freesync_pcon_whitelist_all_known),
	KUNIT_CASE(dm_test_freesync_pcon_whitelist_not_in_list),
	KUNIT_CASE(dm_test_freesync_pcon_whitelist_zero),
	/* populate_hdmi_info_from_connector */
	KUNIT_CASE(dm_test_populate_hdmi_scdc_present_true),
	KUNIT_CASE(dm_test_populate_hdmi_scdc_present_false),
	KUNIT_CASE(dm_test_populate_hdmi_frl_dsc_10bpc),
	KUNIT_CASE(dm_test_populate_hdmi_frl_dsc_12bpc),
	KUNIT_CASE(dm_test_populate_hdmi_frl_dsc_unknown_values),
	/* dm_get_adaptive_sync_support_type */
	KUNIT_CASE(dm_test_adaptive_sync_type_none_default),
	KUNIT_CASE(dm_test_adaptive_sync_type_converter_no_conditions),
	KUNIT_CASE(dm_test_adaptive_sync_type_converter_partial_conditions),
	KUNIT_CASE(dm_test_adaptive_sync_type_pcon_whitelist),
	KUNIT_CASE(dm_test_adaptive_sync_type_converter_nonwhitelist),
	/* dm_helpers_is_fullscreen / dm_helpers_is_hdr_on */
	KUNIT_CASE(dm_test_helpers_is_fullscreen_returns_false),
	KUNIT_CASE(dm_test_helpers_is_hdr_on_returns_false),
	/* get_max_frl_rate */
	KUNIT_CASE(dm_test_get_max_frl_rate_3lanes_3gbps),
	KUNIT_CASE(dm_test_get_max_frl_rate_3lanes_6gbps),
	KUNIT_CASE(dm_test_get_max_frl_rate_4lanes_6gbps),
	KUNIT_CASE(dm_test_get_max_frl_rate_4lanes_8gbps),
	KUNIT_CASE(dm_test_get_max_frl_rate_4lanes_10gbps),
	KUNIT_CASE(dm_test_get_max_frl_rate_4lanes_12gbps),
	KUNIT_CASE(dm_test_get_max_frl_rate_unknown),
	/* dm_dtn_log_begin / dm_dtn_log_append_v / dm_dtn_log_end */
	KUNIT_CASE(dm_test_dtn_log_buffer_accumulates),
	KUNIT_CASE(dm_test_dtn_log_null_ctx_no_crash),
	/* dm_helpers_dp_read_dpcd / dm_helpers_dp_write_dpcd */
	KUNIT_CASE(dm_test_dp_read_dpcd_null_priv),
	KUNIT_CASE(dm_test_dp_write_dpcd_null_priv),
	/* dm_helpers_dp_mst_start_top_mgr / dm_helpers_dp_mst_stop_top_mgr */
	KUNIT_CASE(dm_test_mst_start_top_mgr_null_priv),
	KUNIT_CASE(dm_test_mst_stop_top_mgr_null_priv),
	KUNIT_CASE(dm_test_mst_start_top_mgr_boot),
	/* dm_helpers_dp_write_hblank_reduction */
	KUNIT_CASE(dm_test_dp_write_hblank_reduction_false),
	{}
};

static struct kunit_suite amdgpu_dm_helpers_test_suite = {
	.name = "amdgpu_dm_helpers",
	.test_cases = amdgpu_dm_helpers_test_cases,
};

kunit_test_suite(amdgpu_dm_helpers_test_suite);

MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_helpers");
MODULE_LICENSE("Dual MIT/GPL");
