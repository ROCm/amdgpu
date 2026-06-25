// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_helpers.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <drm/drm_edid.h>
#include <drm/drm_kunit_helpers.h>
#include <drm/display/drm_dp_mst_helper.h>

#include "dc.h"
#include "core_types.h"
#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_mst_types.h"
#include "dc_bios_types.h"
#include "dm_helpers.h"
#include "ddc_service_types.h"
#include "dmub_cmd.h"
#include "amdgpu_dm_helpers.h"
#include "amdgpu_dm_kunit_test_helpers.h"

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

/* Tests for apply_edid_quirks() */

/*
 * Build an EDID whose extracted panel id equals @panel_id. Inverse of
 * edid_extract_panel_id(): mfg_id holds the top 16 bits, prod_code the
 * low 16 bits (prod_code[0] | prod_code[1] << 8).
 */
static struct edid *dm_test_edid_with_panel_id(struct kunit *test, u32 panel_id)
{
	struct edid *edid;

	edid = kunit_kzalloc(test, sizeof(*edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid);

	edid->mfg_id[0] = (panel_id >> 24) & 0xff;
	edid->mfg_id[1] = (panel_id >> 16) & 0xff;
	edid->prod_code[0] = panel_id & 0xff;
	edid->prod_code[1] = (panel_id >> 8) & 0xff;

	return edid;
}

/*
 * Wire a connector-backed link so apply_edid_quirks() can resolve
 * link->priv->base.dev for its drm_dbg_driver() calls.
 */
static struct dc_link *dm_test_quirk_link(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;
	struct dc_link *link;

	adev = dm_kunit_alloc_adev(test);
	link = dm_kunit_alloc_link(test);
	aconnector = dm_kunit_alloc_connector(test, adev, NULL);
	link->priv = aconnector;

	return link;
}

/**
 * dm_test_apply_edid_quirks_dpcd_poweroff_delay - Test GBT 0x3215 delay quirk
 * @test: The KUnit test context
 */
static void dm_test_apply_edid_quirks_dpcd_poweroff_delay(struct kunit *test)
{
	struct dc_edid_caps edid_caps = {0};
	struct dc_link *link = dm_test_quirk_link(test);
	struct edid *edid = dm_test_edid_with_panel_id(test,
			drm_edid_encode_panel_id('G', 'B', 'T', 0x3215));

	apply_edid_quirks(link, edid, &edid_caps);

	KUNIT_EXPECT_EQ(test, edid_caps.panel_patch.wait_after_dpcd_poweroff_ms, 10000U);
}

/**
 * dm_test_apply_edid_quirks_disable_fams - Test SAM panel FAMS-disable quirk
 * @test: The KUnit test context
 */
static void dm_test_apply_edid_quirks_disable_fams(struct kunit *test)
{
	struct dc_edid_caps edid_caps = {0};
	struct dc_link *link = dm_test_quirk_link(test);
	struct edid *edid = dm_test_edid_with_panel_id(test,
			drm_edid_encode_panel_id('S', 'A', 'M', 0x0E5E));

	apply_edid_quirks(link, edid, &edid_caps);

	KUNIT_EXPECT_TRUE(test, edid_caps.panel_patch.disable_fams);
}

/**
 * dm_test_apply_edid_quirks_remove_sink_ext_caps - Test AUO 0x317 clear quirk
 * @test: The KUnit test context
 */
static void dm_test_apply_edid_quirks_remove_sink_ext_caps(struct kunit *test)
{
	struct dc_edid_caps edid_caps = {0};
	struct dc_link *link = dm_test_quirk_link(test);
	struct edid *edid = dm_test_edid_with_panel_id(test,
			drm_edid_encode_panel_id('A', 'U', 'O', 0xA7AB));

	apply_edid_quirks(link, edid, &edid_caps);

	KUNIT_EXPECT_TRUE(test, edid_caps.panel_patch.remove_sink_ext_caps);
}

/**
 * dm_test_apply_edid_quirks_disable_colorimetry - Test SDC VSC-disable quirk
 * @test: The KUnit test context
 */
static void dm_test_apply_edid_quirks_disable_colorimetry(struct kunit *test)
{
	struct dc_edid_caps edid_caps = {0};
	struct dc_link *link = dm_test_quirk_link(test);
	struct edid *edid = dm_test_edid_with_panel_id(test,
			drm_edid_encode_panel_id('S', 'D', 'C', 0x4154));

	apply_edid_quirks(link, edid, &edid_caps);

	KUNIT_EXPECT_TRUE(test, edid_caps.panel_patch.disable_colorimetry);
}

/**
 * dm_test_apply_edid_quirks_skip_phy_ssc - Test DEL 0x4147 PHY SSC quirk
 * @test: The KUnit test context
 */
static void dm_test_apply_edid_quirks_skip_phy_ssc(struct kunit *test)
{
	struct dc_edid_caps edid_caps = {0};
	struct dc_link *link = dm_test_quirk_link(test);
	struct edid *edid = dm_test_edid_with_panel_id(test,
			drm_edid_encode_panel_id('D', 'E', 'L', 0x4147));

	apply_edid_quirks(link, edid, &edid_caps);

	KUNIT_EXPECT_TRUE(test, link->wa_flags.skip_phy_ssc_reduction);
}

/**
 * dm_test_apply_edid_quirks_unknown_noop - Test unknown panel id is a no-op
 * @test: The KUnit test context
 */
static void dm_test_apply_edid_quirks_unknown_noop(struct kunit *test)
{
	struct dc_edid_caps edid_caps = {0};
	struct dc_link *link = dm_test_quirk_link(test);
	struct edid *edid = dm_test_edid_with_panel_id(test,
			drm_edid_encode_panel_id('X', 'Y', 'Z', 0x0000));

	apply_edid_quirks(link, edid, &edid_caps);

	/* default: branch leaves every quirk field untouched */
	KUNIT_EXPECT_EQ(test, edid_caps.panel_patch.wait_after_dpcd_poweroff_ms, 0U);
	KUNIT_EXPECT_FALSE(test, edid_caps.panel_patch.disable_fams);
	KUNIT_EXPECT_FALSE(test, edid_caps.panel_patch.remove_sink_ext_caps);
	KUNIT_EXPECT_FALSE(test, edid_caps.panel_patch.disable_colorimetry);
	KUNIT_EXPECT_FALSE(test, link->wa_flags.skip_phy_ssc_reduction);
}

/* Tests for dm_helpers_parse_edid_caps() */

/*
 * Build a minimal valid base EDID block into @dc_edid. When @good_checksum is
 * false the final checksum byte is corrupted so drm_edid_is_valid() fails.
 *
 *   manufacturer_id = 0xAC10, product_id = 0x1234, serial = 0x12345678,
 *   week = 10, year (raw) = 30, digital input, no CEA extension.
 */
static void dm_test_fill_base_edid(struct dc_edid *dc_edid, bool good_checksum)
{
	static const u8 header[8] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
	};
	u8 *raw = dc_edid->raw_edid;
	int sum = 0;
	int i;

	memset(raw, 0, EDID_LENGTH);
	memcpy(raw, header, sizeof(header));

	raw[8] = 0x10;	raw[9] = 0xAC;		/* mfg_id    -> 0xAC10 */
	raw[10] = 0x34;	raw[11] = 0x12;		/* prod_code -> 0x1234 */
	raw[12] = 0x78;	raw[13] = 0x56;		/* serial    -> 0x12345678 */
	raw[14] = 0x34;	raw[15] = 0x12;
	raw[16] = 10;				/* mfg_week */
	raw[17] = 30;				/* mfg_year (raw) */
	raw[18] = 1;				/* version */
	raw[19] = 4;				/* revision */
	raw[20] = DRM_EDID_INPUT_DIGITAL;	/* digital input */
	raw[126] = 0;				/* no extensions */

	for (i = 0; i < EDID_LENGTH - 1; i++)
		sum += raw[i];
	raw[127] = (256 - (sum % 256)) % 256;
	if (!good_checksum)
		raw[127] ^= 0xFF;		/* corrupt checksum */

	dc_edid->length = EDID_LENGTH;
}

/**
 * dm_test_parse_edid_caps_null_edid - Test NULL edid returns EDID_BAD_INPUT
 * @test: The KUnit test context
 */
static void dm_test_parse_edid_caps_null_edid(struct kunit *test)
{
	struct dc_edid_caps edid_caps = {0};
	struct dc_link *link = dm_test_quirk_link(test);

	KUNIT_EXPECT_EQ(test, dm_helpers_parse_edid_caps(link, NULL, &edid_caps), EDID_BAD_INPUT);
}

/**
 * dm_test_parse_edid_caps_null_caps - Test NULL edid_caps returns EDID_BAD_INPUT
 * @test: The KUnit test context
 */
static void dm_test_parse_edid_caps_null_caps(struct kunit *test)
{
	struct dc_link *link = dm_test_quirk_link(test);
	struct dc_edid *dc_edid;

	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	dm_test_fill_base_edid(dc_edid, true);

	KUNIT_EXPECT_EQ(test, dm_helpers_parse_edid_caps(link, dc_edid, NULL), EDID_BAD_INPUT);
}

/**
 * dm_test_parse_edid_caps_valid - Test field extraction from a valid EDID
 * @test: The KUnit test context
 */
static void dm_test_parse_edid_caps_valid(struct kunit *test)
{
	struct dc_link *link = dm_test_quirk_link(test);
	struct dc_edid_caps *edid_caps;
	struct dc_edid *dc_edid;

	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	edid_caps = kunit_kzalloc(test, sizeof(*edid_caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid_caps);

	dm_test_fill_base_edid(dc_edid, true);

	KUNIT_EXPECT_EQ(test, dm_helpers_parse_edid_caps(link, dc_edid, edid_caps), EDID_OK);
	KUNIT_EXPECT_EQ(test, edid_caps->manufacturer_id, 0xAC10);
	KUNIT_EXPECT_EQ(test, edid_caps->product_id, 0x1234);
	KUNIT_EXPECT_EQ(test, edid_caps->serial_number, 0x12345678U);
	KUNIT_EXPECT_EQ(test, edid_caps->manufacture_week, 10);
	KUNIT_EXPECT_EQ(test, edid_caps->manufacture_year, 30);
	KUNIT_EXPECT_FALSE(test, edid_caps->analog);
}

/**
 * dm_test_parse_edid_caps_bad_checksum - Test bad checksum still parses fields
 * @test: The KUnit test context
 */
static void dm_test_parse_edid_caps_bad_checksum(struct kunit *test)
{
	struct dc_link *link = dm_test_quirk_link(test);
	struct dc_edid_caps *edid_caps;
	struct dc_edid *dc_edid;

	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	edid_caps = kunit_kzalloc(test, sizeof(*edid_caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid_caps);

	dm_test_fill_base_edid(dc_edid, false);

	/* Invalid checksum -> EDID_BAD_CHECKSUM but fields are still parsed */
	KUNIT_EXPECT_EQ(test,
			dm_helpers_parse_edid_caps(link, dc_edid, edid_caps),
			EDID_BAD_CHECKSUM);
	KUNIT_EXPECT_EQ(test, edid_caps->manufacturer_id, 0xAC10);
	KUNIT_EXPECT_EQ(test, edid_caps->product_id, 0x1234);
}

/**
 * dm_test_parse_edid_caps_hdmi_frl - Test HDMI/FRL branch via connector info
 * @test: The KUnit test context
 *
 * Drives the edid_caps->edid_hdmi path by marking the connector as HDMI and
 * providing a fake dc with FRL enabled, so populate_hdmi_info_from_connector()
 * is exercised inside dm_helpers_parse_edid_caps().
 */
static void dm_test_parse_edid_caps_hdmi_frl(struct kunit *test)
{
	struct dc_link *link = dm_test_quirk_link(test);
	struct amdgpu_dm_connector *aconnector = link->priv;
	struct drm_connector *connector = &aconnector->base;
	struct dc_edid_caps *edid_caps;
	struct dc_edid *dc_edid;
	struct dc *dc;

	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	edid_caps = kunit_kzalloc(test, sizeof(*edid_caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid_caps);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc);

	link->dc = dc;
	dc->config.enable_frl = true;

	dm_test_fill_base_edid(dc_edid, true);

	/* Drive the HDMI/FRL branch */
	connector->display_info.is_hdmi = true;
	connector->display_info.hdmi.scdc.supported = true;
	connector->display_info.hdmi.max_lanes = 4;
	connector->display_info.hdmi.max_frl_rate_per_lane = 12;

	KUNIT_EXPECT_EQ(test, dm_helpers_parse_edid_caps(link, dc_edid, edid_caps), EDID_OK);
	KUNIT_EXPECT_TRUE(test, edid_caps->edid_hdmi);
	KUNIT_EXPECT_TRUE(test, edid_caps->scdc_present);
	/* max_lanes 4 + max_frl_rate_per_lane 12 -> rate index 6 */
	KUNIT_EXPECT_EQ(test, edid_caps->max_frl_rate, 6);
}

/**
 * dm_test_parse_edid_caps_hdmi_frl_dsc - Test HDMI FRL DSC sub-branch
 * @test: The KUnit test context
 *
 * Sets the connector's HDMI DSC capability so populate_hdmi_info_from_connector()
 * reports frl_dsc_support, exercising the frl_dsc_support log path.
 */
static void dm_test_parse_edid_caps_hdmi_frl_dsc(struct kunit *test)
{
	struct dc_link *link = dm_test_quirk_link(test);
	struct amdgpu_dm_connector *aconnector = link->priv;
	struct drm_connector *connector = &aconnector->base;
	struct dc_edid_caps *edid_caps;
	struct dc_edid *dc_edid;
	struct dc *dc;

	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	edid_caps = kunit_kzalloc(test, sizeof(*edid_caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid_caps);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc);

	link->dc = dc;
	dc->config.enable_frl = true;

	dm_test_fill_base_edid(dc_edid, true);

	connector->display_info.is_hdmi = true;
	/* Drive the frl_dsc_support sub-branch */
	connector->display_info.hdmi.dsc_cap.v_1p2 = true;
	connector->display_info.hdmi.dsc_cap.bpc_supported = 10;

	KUNIT_EXPECT_EQ(test, dm_helpers_parse_edid_caps(link, dc_edid, edid_caps), EDID_OK);
	KUNIT_EXPECT_TRUE(test, edid_caps->frl_dsc_support);
	KUNIT_EXPECT_TRUE(test, edid_caps->frl_dsc_10bpc);
}

/*
 * Build a 2-block EDID: a valid base block plus a CTA-861 extension block
 * carrying one LPCM Short Audio Descriptor and, when @with_speaker is set, a
 * Speaker Allocation Data Block, so the audio/speaker parsing code is
 * exercised.
 */
static void dm_test_fill_cea_edid(struct dc_edid *dc_edid, bool with_speaker)
{
	u8 *raw = dc_edid->raw_edid;
	u8 *ext = raw + EDID_LENGTH;
	u8 dtd_offset;
	int sum = 0;
	int i;

	/* Base block, flagged as having one extension */
	dm_test_fill_base_edid(dc_edid, true);
	raw[126] = 1;
	for (i = 0; i < EDID_LENGTH - 1; i++)
		sum += raw[i];
	raw[127] = (256 - (sum % 256)) % 256;

	/* Audio DB spans [4,7]; optional Speaker Alloc DB spans [8,11] */
	dtd_offset = with_speaker ? 12 : 8;

	/* CTA-861 extension block */
	memset(ext, 0, EDID_LENGTH);
	ext[0] = 0x02;			/* CTA extension tag */
	ext[1] = 0x03;			/* revision 3 */
	ext[2] = dtd_offset;		/* DTD offset; end of data blocks */
	ext[3] = 0x00;			/* no native DTDs / feature flags */
	/* Audio Data Block: tag 1, length 3, one LPCM SAD */
	ext[4] = (1 << 5) | 3;
	ext[5] = (1 << 3) | 1;		/* format 1 (LPCM), 2 channels */
	ext[6] = 0x07;			/* 32 / 44.1 / 48 kHz */
	ext[7] = 0x07;			/* 16 / 20 / 24-bit */
	if (with_speaker) {
		/* Speaker Allocation Data Block: tag 4, length 3 */
		ext[8] = (4 << 5) | 3;
		ext[9] = 0x01;		/* front left / front right */
	}

	sum = 0;
	for (i = 0; i < EDID_LENGTH - 1; i++)
		sum += ext[i];
	ext[127] = (256 - (sum % 256)) % 256;

	dc_edid->length = 2 * EDID_LENGTH;
}

/**
 * dm_test_parse_edid_caps_cea_audio - Test CEA audio/speaker block parsing
 * @test: The KUnit test context
 */
static void dm_test_parse_edid_caps_cea_audio(struct kunit *test)
{
	struct dc_link *link = dm_test_quirk_link(test);
	struct dc_edid_caps *edid_caps;
	struct dc_edid *dc_edid;

	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	edid_caps = kunit_kzalloc(test, sizeof(*edid_caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid_caps);

	dm_test_fill_cea_edid(dc_edid, true);

	KUNIT_EXPECT_EQ(test, dm_helpers_parse_edid_caps(link, dc_edid, edid_caps), EDID_OK);
	KUNIT_EXPECT_EQ(test, edid_caps->audio_mode_count, 1);
	KUNIT_EXPECT_EQ(test, edid_caps->audio_modes[0].format_code, 1);
	KUNIT_EXPECT_EQ(test, edid_caps->audio_modes[0].channel_count, 2);
	KUNIT_EXPECT_EQ(test, edid_caps->audio_modes[0].sample_rate, 0x07);
	KUNIT_EXPECT_EQ(test, edid_caps->audio_modes[0].sample_size, 0x07);
	KUNIT_EXPECT_EQ(test, edid_caps->speaker_flags, 0x01);
}

/**
 * dm_test_parse_edid_caps_cea_no_speaker - Test default speaker flags path
 * @test: The KUnit test context
 *
 * Audio data block present but no Speaker Allocation Data Block, so
 * speaker_flags falls back to DEFAULT_SPEAKER_LOCATION.
 */
static void dm_test_parse_edid_caps_cea_no_speaker(struct kunit *test)
{
	struct dc_link *link = dm_test_quirk_link(test);
	struct dc_edid_caps *edid_caps;
	struct dc_edid *dc_edid;

	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	edid_caps = kunit_kzalloc(test, sizeof(*edid_caps), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, edid_caps);

	dm_test_fill_cea_edid(dc_edid, false);

	KUNIT_EXPECT_EQ(test, dm_helpers_parse_edid_caps(link, dc_edid, edid_caps), EDID_OK);
	KUNIT_EXPECT_EQ(test, edid_caps->audio_mode_count, 1);
	KUNIT_EXPECT_EQ(test, edid_caps->speaker_flags, DEFAULT_SPEAKER_LOCATION);
}

/* Tests for ACPI and VBIOS EDID readers */

/**
 * dm_test_probe_acpi_edid_no_companion - Test ACPI probe without companion
 * @test: The KUnit test context
 */
static void dm_test_probe_acpi_edid_no_companion(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;
	u8 buf[EDID_LENGTH] = {0};

	adev = dm_kunit_alloc_adev(test);
	aconnector = dm_kunit_alloc_connector(test, adev, NULL);

	KUNIT_EXPECT_EQ(test,
			dm_helpers_probe_acpi_edid(&aconnector->base, buf, 0, sizeof(buf)),
			-ENODEV);
}

/**
 * dm_test_read_acpi_edid_debug_mask_disabled - Test debug mask early return
 * @test: The KUnit test context
 */
static void dm_test_read_acpi_edid_debug_mask_disabled(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;
	uint old_debug_mask;

	adev = dm_kunit_alloc_adev(test);
	aconnector = dm_kunit_alloc_connector(test, adev, NULL);
	old_debug_mask = dm_helpers_get_dc_debug_mask();

	dm_helpers_set_dc_debug_mask(old_debug_mask | DC_DISABLE_ACPI_EDID);
	KUNIT_EXPECT_NULL(test, dm_helpers_read_acpi_edid(aconnector));
	dm_helpers_set_dc_debug_mask(old_debug_mask);
}

/**
 * dm_test_read_acpi_edid_non_panel_connector - Test non-panel connector skip
 * @test: The KUnit test context
 */
static void dm_test_read_acpi_edid_non_panel_connector(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;

	adev = dm_kunit_alloc_adev(test);
	aconnector = dm_kunit_alloc_connector(test, adev, NULL);
	aconnector->base.connector_type = DRM_MODE_CONNECTOR_HDMIA;

	KUNIT_EXPECT_NULL(test, dm_helpers_read_acpi_edid(aconnector));
}

/**
 * dm_test_read_acpi_edid_force_off - Test forced-off connector skip
 * @test: The KUnit test context
 */
static void dm_test_read_acpi_edid_force_off(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;

	adev = dm_kunit_alloc_adev(test);
	aconnector = dm_kunit_alloc_connector(test, adev, NULL);
	aconnector->base.connector_type = DRM_MODE_CONNECTOR_eDP;
	aconnector->base.force = DRM_FORCE_OFF;

	KUNIT_EXPECT_NULL(test, dm_helpers_read_acpi_edid(aconnector));
}

struct dm_test_vbios_edid {
	struct dc_bios bios;
	enum bp_result result;
	const u8 *fake_edid;
	u16 fake_edid_size;
	u16 width_mm;
	u16 height_mm;
};

static enum bp_result dm_test_get_embedded_panel_info(struct dc_bios *bios,
						      struct embedded_panel_info *info)
{
	struct dm_test_vbios_edid *fixture;

	fixture = container_of(bios, struct dm_test_vbios_edid, bios);
	if (fixture->result != BP_RESULT_OK)
		return fixture->result;

	info->fake_edid = fixture->fake_edid;
	info->fake_edid_size = fixture->fake_edid_size;
	info->panel_width_mm = fixture->width_mm;
	info->panel_height_mm = fixture->height_mm;

	return BP_RESULT_OK;
}

static const struct dc_vbios_funcs dm_test_vbios_edid_funcs = {
	.get_embedded_panel_info = dm_test_get_embedded_panel_info,
};

static void dm_test_setup_vbios_link(struct kunit *test,
				     struct dc_link **link_out,
				     struct amdgpu_dm_connector **aconnector_out,
				     struct dm_test_vbios_edid **fixture_out)
{
	struct dm_test_vbios_edid *fixture;
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;
	struct dc_context *ctx;
	struct dc_link *link;

	adev = dm_kunit_alloc_adev(test);
	link = dm_kunit_alloc_link(test);
	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	fixture = kunit_kzalloc(test, sizeof(*fixture), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, fixture);
	aconnector = dm_kunit_alloc_connector(test, adev, link);

	fixture->bios.funcs = &dm_test_vbios_edid_funcs;
	fixture->result = BP_RESULT_OK;
	ctx->dc_bios = &fixture->bios;
	link->ctx = ctx;
	link->priv = aconnector;
	link->connector_signal = SIGNAL_TYPE_EDP;

	*link_out = link;
	*aconnector_out = aconnector;
	*fixture_out = fixture;
}

/**
 * dm_test_read_vbios_edid_non_embedded - Test non-embedded signal skip
 * @test: The KUnit test context
 */
static void dm_test_read_vbios_edid_non_embedded(struct kunit *test)
{
	struct dm_test_vbios_edid *fixture;
	struct amdgpu_dm_connector *aconnector;
	struct dc_link *link;

	dm_test_setup_vbios_link(test, &link, &aconnector, &fixture);
	link->connector_signal = SIGNAL_TYPE_DISPLAY_PORT;

	KUNIT_EXPECT_NULL(test,
			  dm_helpers_read_vbios_hardcoded_edid(link, aconnector));
}

/**
 * dm_test_read_vbios_edid_missing_callback - Test missing callback skip
 * @test: The KUnit test context
 */
static void dm_test_read_vbios_edid_missing_callback(struct kunit *test)
{
	static const struct dc_vbios_funcs empty_funcs;
	struct dm_test_vbios_edid *fixture;
	struct amdgpu_dm_connector *aconnector;
	struct dc_link *link;

	dm_test_setup_vbios_link(test, &link, &aconnector, &fixture);
	fixture->bios.funcs = &empty_funcs;

	KUNIT_EXPECT_NULL(test,
			  dm_helpers_read_vbios_hardcoded_edid(link, aconnector));
}

/**
 * dm_test_read_vbios_edid_callback_error - Test callback failure skip
 * @test: The KUnit test context
 */
static void dm_test_read_vbios_edid_callback_error(struct kunit *test)
{
	struct dm_test_vbios_edid *fixture;
	struct amdgpu_dm_connector *aconnector;
	struct dc_link *link;

	dm_test_setup_vbios_link(test, &link, &aconnector, &fixture);
	fixture->result = BP_RESULT_BADINPUT;

	KUNIT_EXPECT_NULL(test,
			  dm_helpers_read_vbios_hardcoded_edid(link, aconnector));
}

/**
 * dm_test_read_vbios_edid_missing_fake_edid - Test missing fake EDID skip
 * @test: The KUnit test context
 */
static void dm_test_read_vbios_edid_missing_fake_edid(struct kunit *test)
{
	struct dm_test_vbios_edid *fixture;
	struct amdgpu_dm_connector *aconnector;
	struct dc_link *link;

	dm_test_setup_vbios_link(test, &link, &aconnector, &fixture);
	fixture->fake_edid = NULL;
	fixture->fake_edid_size = 0;

	KUNIT_EXPECT_NULL(test,
			  dm_helpers_read_vbios_hardcoded_edid(link, aconnector));
}

/**
 * dm_test_read_vbios_edid_invalid_fake_edid - Test invalid fake EDID skip
 * @test: The KUnit test context
 */
static void dm_test_read_vbios_edid_invalid_fake_edid(struct kunit *test)
{
	struct dm_test_vbios_edid *fixture;
	struct amdgpu_dm_connector *aconnector;
	struct dc_edid *dc_edid;
	struct dc_link *link;

	dm_test_setup_vbios_link(test, &link, &aconnector, &fixture);
	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	dm_test_fill_base_edid(dc_edid, false);
	fixture->fake_edid = dc_edid->raw_edid;
	fixture->fake_edid_size = EDID_LENGTH;

	KUNIT_EXPECT_NULL(test,
			  dm_helpers_read_vbios_hardcoded_edid(link, aconnector));
}

/**
 * dm_test_read_vbios_edid_valid - Test valid fake EDID updates display size
 * @test: The KUnit test context
 */
static void dm_test_read_vbios_edid_valid(struct kunit *test)
{
	struct dm_test_vbios_edid *fixture;
	struct amdgpu_dm_connector *aconnector;
	const struct drm_edid *edid;
	struct dc_edid *dc_edid;
	struct dc_link *link;

	dm_test_setup_vbios_link(test, &link, &aconnector, &fixture);
	dc_edid = kunit_kzalloc(test, sizeof(*dc_edid), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc_edid);
	dm_test_fill_base_edid(dc_edid, true);
	fixture->fake_edid = dc_edid->raw_edid;
	fixture->fake_edid_size = EDID_LENGTH;
	fixture->width_mm = 301;
	fixture->height_mm = 201;

	edid = dm_helpers_read_vbios_hardcoded_edid(link, aconnector);

	KUNIT_ASSERT_NOT_NULL(test, edid);
	KUNIT_EXPECT_EQ(test, aconnector->base.display_info.width_mm, 301U);
	KUNIT_EXPECT_EQ(test, aconnector->base.display_info.height_mm, 201U);
	drm_edid_free(edid);
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

/*
 * Stub AUX transfer that ACKs every transaction (zero-filling reads), so
 * drm_dp_dpcd_read()/drm_dp_dpcd_write() report the full transfer size.
 */
static ssize_t dm_test_dpcd_ack_transfer(struct drm_dp_aux *aux,
					 struct drm_dp_aux_msg *msg)
{
	if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_READ)
		memset(msg->buffer, 0, msg->size);
	msg->reply = DP_AUX_NATIVE_REPLY_ACK;
	return msg->size;
}

/*
 * Wire a connector-backed link with a working AUX channel so the DPCD
 * read/write helpers can complete a real transaction.
 */
static struct dc_link *dm_test_dpcd_link(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct amdgpu_device *adev;
	struct dc_link *link;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	link = dm_kunit_alloc_link(test);
	aconnector = dm_kunit_alloc_connector(test, adev, NULL);

	aconnector->dm_dp_aux.aux.drm_dev = &adev->ddev;
	aconnector->dm_dp_aux.aux.transfer = dm_test_dpcd_ack_transfer;
	drm_dp_aux_init(&aconnector->dm_dp_aux.aux);

	link->priv = aconnector;

	return link;
}

/**
 * dm_test_dp_read_dpcd_success - Test DPCD read returns true on ACKed transfer
 * @test: The KUnit test context
 */
static void dm_test_dp_read_dpcd_success(struct kunit *test)
{
	struct dc_link *link = dm_test_dpcd_link(test);
	uint8_t data = 0;

	KUNIT_EXPECT_TRUE(test,
			  dm_helpers_dp_read_dpcd(NULL, link, 0, &data, sizeof(data)));
}

/**
 * dm_test_dp_write_dpcd_success - Test DPCD write returns true on ACKed transfer
 * @test: The KUnit test context
 */
static void dm_test_dp_write_dpcd_success(struct kunit *test)
{
	struct dc_link *link = dm_test_dpcd_link(test);
	uint8_t data = 0;

	KUNIT_EXPECT_TRUE(test,
			  dm_helpers_dp_write_dpcd(NULL, link, 0, &data, sizeof(data)));
}

/* Tests for dm_helpers_execute_fused_io() */

/**
 * dm_test_execute_fused_io_null_dmub_srv - Test fused IO fails without DMUB service
 * @test: The KUnit test context
 */
static void dm_test_execute_fused_io_null_dmub_srv(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_context *ctx;
	struct dc_link *link;
	union dmub_rb_cmd *commands;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_NOT_NULL(test, adev);
	mutex_init(&adev->dm.dpia_aux_lock);
	spin_lock_init(&adev->dm.dmub_lock);

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);
	commands = kunit_kzalloc(test, sizeof(*commands), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, commands);

	ctx->driver_context = adev;
	link->ctx = ctx;
	commands[0].fused_io.request.u.aux.ddc_line = 0;

	KUNIT_EXPECT_FALSE(test, dm_helpers_execute_fused_io(ctx, link, commands, 1, 1));
}

struct dm_test_synaptics_aux {
	struct drm_dp_aux aux;
	u32 fail_address;
	u32 last_dpcd_write_address;
	u8 rc_result;
	u8 dpcd_read_value;
	u8 dpcd_write_value;
	u8 last_rc_data[16];
	u8 read_rc_data[16];
	u8 last_rc_command;
	u8 rc_commands[32];
	u8 dsc_enable_values[8];
	u32 last_rc_offset;
	u32 last_rc_length;
	unsigned int rc_data_writes;
	unsigned int rc_data_reads;
	unsigned int rc_command_reads;
	unsigned int rc_result_reads;
	unsigned int rc_command_count;
	unsigned int downspread_reads;
	unsigned int downspread_writes;
	unsigned int dsc_enable_writes;
};

static ssize_t dm_test_synaptics_aux_transfer(struct drm_dp_aux *aux,
					      struct drm_dp_aux_msg *msg)
{
	struct dm_test_synaptics_aux *fixture;
	u8 request;
	u8 *buffer;
	size_t copy_size;
	unsigned int index;

	fixture = container_of(aux, struct dm_test_synaptics_aux, aux);
	request = msg->request & ~DP_AUX_I2C_MOT;
	buffer = msg->buffer;

	if (fixture->fail_address == msg->address)
		return -EIO;

	if (request == DP_AUX_NATIVE_WRITE) {
		switch (msg->address) {
		case DP_DOWNSPREAD_CTRL:
			fixture->last_dpcd_write_address = msg->address;
			if (msg->size)
				fixture->dpcd_write_value = buffer[0];
			fixture->downspread_writes++;
			break;
		case SYNAPTICS_RC_DATA:
			copy_size = min_t(size_t, msg->size, sizeof(fixture->last_rc_data));
			memset(fixture->last_rc_data, 0, sizeof(fixture->last_rc_data));
			memcpy(fixture->last_rc_data, buffer, copy_size);
			fixture->rc_data_writes++;
			break;
		case SYNAPTICS_RC_OFFSET:
			if (msg->size >= 4)
				fixture->last_rc_offset = buffer[0] | buffer[1] << 8 |
							  buffer[2] << 16 | buffer[3] << 24;
			break;
		case SYNAPTICS_RC_LENGTH:
			if (msg->size >= 2)
				fixture->last_rc_length = buffer[0] | buffer[1] << 8;
			break;
		case SYNAPTICS_RC_COMMAND:
			fixture->last_rc_command = buffer[0];
			if (fixture->rc_command_count < ARRAY_SIZE(fixture->rc_commands)) {
				fixture->rc_commands[fixture->rc_command_count] = buffer[0] & 0x7f;
				fixture->rc_command_count++;
			}
			break;
		case DP_DSC_ENABLE:
			if (fixture->dsc_enable_writes < ARRAY_SIZE(fixture->dsc_enable_values)) {
				fixture->dsc_enable_values[fixture->dsc_enable_writes] = buffer[0];
				fixture->dsc_enable_writes++;
			}
			break;
		}
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;
		return msg->size;
	}

	if (request == DP_AUX_NATIVE_READ) {
		memset(buffer, 0, msg->size);
		switch (msg->address) {
		case DP_DOWNSPREAD_CTRL:
			if (msg->size)
				buffer[0] = fixture->dpcd_read_value;
			fixture->downspread_reads++;
			break;
		case SYNAPTICS_RC_COMMAND:
			if (msg->size)
				buffer[0] = fixture->last_rc_command & 0x7f;
			fixture->rc_command_reads++;
			break;
		case SYNAPTICS_RC_RESULT:
			if (msg->size)
				buffer[0] = fixture->rc_result;
			fixture->rc_result_reads++;
			break;
		case SYNAPTICS_RC_DATA:
			copy_size = min_t(size_t, msg->size, sizeof(fixture->read_rc_data));
			for (index = 0; index < copy_size; index++)
				buffer[index] = fixture->read_rc_data[index];
			fixture->rc_data_reads++;
			break;
		}
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;
		return msg->size;
	}

	msg->reply = DP_AUX_NATIVE_REPLY_ACK;
	return msg->size;
}

static struct dm_test_synaptics_aux *dm_test_alloc_synaptics_aux_with_dev(struct kunit *test,
								 struct drm_device *drm_dev)
{
	struct dm_test_synaptics_aux *fixture;
	unsigned int index;

	fixture = kunit_kzalloc(test, sizeof(*fixture), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, fixture);

	for (index = 0; index < ARRAY_SIZE(fixture->read_rc_data); index++)
		fixture->read_rc_data[index] = 0x03;

	fixture->rc_result = 0;
	fixture->aux.drm_dev = drm_dev;
	fixture->aux.transfer = dm_test_synaptics_aux_transfer;
	drm_dp_aux_init(&fixture->aux);

	return fixture;
}

static struct dm_test_synaptics_aux *dm_test_alloc_synaptics_aux(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_NOT_NULL(test, adev);

	return dm_test_alloc_synaptics_aux_with_dev(test, &adev->ddev);
}

static void dm_test_expect_synaptics_commands(struct kunit *test,
					      struct dm_test_synaptics_aux *fixture,
					      const u8 *expected_commands,
					      unsigned int expected_count)
{
	unsigned int index;

	KUNIT_ASSERT_EQ(test, fixture->rc_command_count, expected_count);

	for (index = 0; index < expected_count; index++)
		KUNIT_EXPECT_EQ(test, fixture->rc_commands[index], expected_commands[index]);
}

static void dm_test_setup_synaptics_stream(struct dc_stream_state *stream,
						  struct dc_link *link)
{
	stream->link = link;
	stream->signal = SIGNAL_TYPE_DISPLAY_PORT_MST;
	link->dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_90CC24;
	link->dpcd_caps.dpcd_rev.raw = DP_DPCD_REV_14;
	link->dpcd_caps.sink_count.bits.SINK_COUNT = 2;
	memcpy(link->dpcd_caps.branch_dev_name, "SYNA", 4);
}

/**
 * dm_test_execute_synaptics_rc_command_write_success - Test RC write success
 * @test: The KUnit test context
 */
static void dm_test_execute_synaptics_rc_command_write_success(struct kunit *test)
{
	struct dm_test_synaptics_aux *fixture;
	u8 data[5] = { 'P', 'R', 'I', 'U', 'S' };

	fixture = dm_test_alloc_synaptics_aux(test);

	KUNIT_EXPECT_TRUE(test, execute_synaptics_rc_command(&fixture->aux, true,
							     0x01, sizeof(data), 0x123456,
							     data));
	KUNIT_EXPECT_EQ(test, memcmp(fixture->last_rc_data, data, sizeof(data)), 0);
	KUNIT_EXPECT_EQ(test, fixture->last_rc_offset, 0x123456U);
	KUNIT_EXPECT_EQ(test, fixture->last_rc_length, (u32)sizeof(data));
	KUNIT_EXPECT_EQ(test, fixture->last_rc_command, (u8)0x81);
	KUNIT_EXPECT_EQ(test, fixture->rc_command_reads, 1U);
	KUNIT_EXPECT_EQ(test, fixture->rc_result_reads, 1U);
}

/**
 * dm_test_execute_synaptics_rc_command_read_success - Test RC read success
 * @test: The KUnit test context
 */
static void dm_test_execute_synaptics_rc_command_read_success(struct kunit *test)
{
	struct dm_test_synaptics_aux *fixture;
	u8 data[4] = { 0 };
	u8 expected[4] = { 0xa5, 0x5a, 0xc3, 0x3c };

	fixture = dm_test_alloc_synaptics_aux(test);
	memcpy(fixture->read_rc_data, expected, sizeof(expected));

	KUNIT_EXPECT_TRUE(test, execute_synaptics_rc_command(&fixture->aux, false,
							     0x31, sizeof(data), 0x220998,
							     data));
	KUNIT_EXPECT_EQ(test, memcmp(data, expected, sizeof(expected)), 0);
	KUNIT_EXPECT_EQ(test, fixture->rc_data_writes, 0U);
	KUNIT_EXPECT_EQ(test, fixture->rc_data_reads, 1U);
	KUNIT_EXPECT_EQ(test, fixture->last_rc_offset, 0x220998U);
	KUNIT_EXPECT_EQ(test, fixture->last_rc_length, (u32)sizeof(data));
}

/**
 * dm_test_execute_synaptics_rc_command_write_fail - Test RC write failure
 * @test: The KUnit test context
 */
static void dm_test_execute_synaptics_rc_command_write_fail(struct kunit *test)
{
	struct dm_test_synaptics_aux *fixture;
	u8 data = 0;

	fixture = dm_test_alloc_synaptics_aux(test);
	fixture->fail_address = SYNAPTICS_RC_LENGTH;

	KUNIT_EXPECT_FALSE(test, execute_synaptics_rc_command(&fixture->aux, true,
							      0x01, sizeof(data), 0, &data));
	KUNIT_EXPECT_EQ(test, fixture->rc_command_count, 0U);
}

/**
 * dm_test_apply_synaptics_fifo_reset_wa_full - Test full FIFO reset sequence
 * @test: The KUnit test context
 */
static void dm_test_apply_synaptics_fifo_reset_wa_full(struct kunit *test)
{
	static const u8 expected_commands[] = {
		0x01, 0x31, 0x21, 0x31, 0x21, 0x31, 0x21,
		0x31, 0x21, 0x31, 0x31, 0x21, 0x02,
	};
	struct dm_test_synaptics_aux *fixture;

	fixture = dm_test_alloc_synaptics_aux(test);

	apply_synaptics_fifo_reset_wa(&fixture->aux);

	dm_test_expect_synaptics_commands(test, fixture, expected_commands,
					  ARRAY_SIZE(expected_commands));
	KUNIT_EXPECT_EQ(test, fixture->rc_result_reads, (unsigned int)ARRAY_SIZE(expected_commands));
}

/**
 * dm_test_apply_synaptics_fifo_reset_wa_first_fail - Test FIFO reset early exit
 * @test: The KUnit test context
 */
static void dm_test_apply_synaptics_fifo_reset_wa_first_fail(struct kunit *test)
{
	struct dm_test_synaptics_aux *fixture;

	fixture = dm_test_alloc_synaptics_aux(test);
	fixture->rc_result = 0xff;

	apply_synaptics_fifo_reset_wa(&fixture->aux);

	KUNIT_EXPECT_EQ(test, fixture->rc_command_count, 1U);
	KUNIT_EXPECT_EQ(test, fixture->rc_commands[0], (u8)0x01);
}

static void dm_test_write_dsc_enable_synaptics(struct kunit *test,
					       bool link_active,
					       bool enable,
					       bool synaptics_branch,
					       unsigned int expected_dsc_writes,
					       unsigned int expected_rc_commands)
{
	struct dm_test_synaptics_aux *fixture;
	struct dc_stream_state *stream;
	struct dc_link *link;
	u8 ret;

	fixture = dm_test_alloc_synaptics_aux(test);
	stream = kunit_kzalloc(test, sizeof(*stream), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, stream);
	link = dm_kunit_alloc_link(test);

	dm_test_setup_synaptics_stream(stream, link);
	link->link_status.link_active = link_active;
	if (!synaptics_branch)
		memcpy(link->dpcd_caps.branch_dev_name, "ABCD", 4);

	ret = write_dsc_enable_synaptics_non_virtual_dpcd_mst(&fixture->aux, stream, enable);

	KUNIT_EXPECT_EQ(test, ret, expected_dsc_writes ? 1 : 0);
	KUNIT_EXPECT_EQ(test, fixture->dsc_enable_writes, expected_dsc_writes);
	KUNIT_EXPECT_EQ(test, fixture->rc_command_count, expected_rc_commands);
	if (expected_dsc_writes)
		KUNIT_EXPECT_EQ(test, fixture->dsc_enable_values[0], enable ? 1 : 0);
}

/**
 * dm_test_write_dsc_enable_synaptics_enable_inactive - Test enable plus FIFO reset
 * @test: The KUnit test context
 */
static void dm_test_write_dsc_enable_synaptics_enable_inactive(struct kunit *test)
{
	dm_test_write_dsc_enable_synaptics(test, false, true, true, 1, 13);
}

/**
 * dm_test_write_dsc_enable_synaptics_enable_active - Test enable skips FIFO reset
 * @test: The KUnit test context
 */
static void dm_test_write_dsc_enable_synaptics_enable_active(struct kunit *test)
{
	dm_test_write_dsc_enable_synaptics(test, true, true, true, 1, 0);
}

/**
 * dm_test_write_dsc_enable_synaptics_disable_inactive - Test inactive disable writes DPCD
 * @test: The KUnit test context
 */
static void dm_test_write_dsc_enable_synaptics_disable_inactive(struct kunit *test)
{
	dm_test_write_dsc_enable_synaptics(test, false, false, true, 1, 0);
}

/**
 * dm_test_write_dsc_enable_synaptics_disable_active - Test active disable skips DPCD
 * @test: The KUnit test context
 */
static void dm_test_write_dsc_enable_synaptics_disable_active(struct kunit *test)
{
	dm_test_write_dsc_enable_synaptics(test, true, false, true, 0, 0);
}

/**
 * dm_test_write_dsc_enable_synaptics_enable_non_synaptics - Test non-Synaptics enable
 * @test: The KUnit test context
 */
static void dm_test_write_dsc_enable_synaptics_enable_non_synaptics(struct kunit *test)
{
	dm_test_write_dsc_enable_synaptics(test, false, true, false, 1, 0);
}

/**
 * dm_test_dp_write_dsc_enable_routes_synaptics - Test public DSC helper workaround route
 * @test: The KUnit test context
 */
static void dm_test_dp_write_dsc_enable_routes_synaptics(struct kunit *test)
{
	struct dm_test_synaptics_aux *fixture;
	struct amdgpu_dm_connector *aconnector;
	struct dc_stream_state *stream;
	struct dc_link *link;

	fixture = dm_test_alloc_synaptics_aux(test);
	aconnector = kunit_kzalloc(test, sizeof(*aconnector), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, aconnector);
	stream = kunit_kzalloc(test, sizeof(*stream), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, stream);
	link = dm_kunit_alloc_link(test);

	dm_test_setup_synaptics_stream(stream, link);
	stream->dm_stream_context = aconnector;
	aconnector->dc_link = link;
	aconnector->dsc_aux = &fixture->aux;

	KUNIT_EXPECT_TRUE(test, dm_helpers_dp_write_dsc_enable(NULL, stream, true));
	KUNIT_EXPECT_EQ(test, fixture->dsc_enable_writes, 1U);
	KUNIT_EXPECT_EQ(test, fixture->dsc_enable_values[0], (u8)1);
	KUNIT_EXPECT_EQ(test, fixture->rc_command_count, 13U);
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
	struct dc_link *link;

	adev = dm_kunit_alloc_adev(test);

	link = dm_kunit_alloc_link(test);

	aconnector = dm_kunit_alloc_connector(test, adev, NULL);

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

/* Tests for get_dsc_max_slices() */

/**
 * dm_test_get_dsc_max_slices_1_340 - Test 1 slice at 340 MHz
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_1_340(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(1, 340), 1);
}

/**
 * dm_test_get_dsc_max_slices_2_340 - Test 2 slices at 340 MHz
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_2_340(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(2, 340), 2);
}

/**
 * dm_test_get_dsc_max_slices_4_340 - Test 4 slices at 340 MHz
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_4_340(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(4, 340), 3);
}

/**
 * dm_test_get_dsc_max_slices_8_340 - Test 8 slices at 340 MHz
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_8_340(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(8, 340), 4);
}

/**
 * dm_test_get_dsc_max_slices_8_400 - Test 8 slices at 400 MHz
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_8_400(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(8, 400), 5);
}

/**
 * dm_test_get_dsc_max_slices_12_400 - Test 12 slices at 400 MHz
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_12_400(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(12, 400), 6);
}

/**
 * dm_test_get_dsc_max_slices_16_400 - Test 16 slices at 400 MHz
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_16_400(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(16, 400), 7);
}

/**
 * dm_test_get_dsc_max_slices_unknown - Test unknown combination returns 0
 * @test: The KUnit test context
 */
static void dm_test_get_dsc_max_slices_unknown(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(3, 340), 0);
	KUNIT_EXPECT_EQ(test, get_dsc_max_slices(1, 400), 0);
}

/* Tests for dm_helpers_init_panel_settings() */

/**
 * dm_test_init_panel_settings_pps - Test panel power sequence settings init
 * @test: The KUnit test context
 */
static void dm_test_init_panel_settings_pps(struct kunit *test)
{
	struct dc_panel_config panel_config = {0};
	struct dc_sink *sink;

	sink = kunit_kzalloc(test, sizeof(*sink), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, sink);

	sink->edid_caps.panel_patch.extra_t3_ms = 100;
	sink->edid_caps.panel_patch.extra_t7_ms = 200;
	sink->edid_caps.panel_patch.extra_delay_backlight_off = 50;
	sink->edid_caps.panel_patch.extra_t12_ms = 300;

	dm_helpers_init_panel_settings(NULL, &panel_config, sink);

	KUNIT_EXPECT_EQ(test, panel_config.pps.extra_t3_ms, 100U);
	KUNIT_EXPECT_EQ(test, panel_config.pps.extra_t7_ms, 200U);
	KUNIT_EXPECT_EQ(test, panel_config.pps.extra_delay_backlight_off, 50U);
	KUNIT_EXPECT_EQ(test, panel_config.pps.extra_post_t7_ms, 0U);
	KUNIT_EXPECT_EQ(test, panel_config.pps.extra_pre_t11_ms, 0U);
	KUNIT_EXPECT_EQ(test, panel_config.pps.extra_t12_ms, 300U);
	KUNIT_EXPECT_EQ(test, panel_config.pps.extra_post_OUI_ms, 0U);
}

/**
 * dm_test_init_panel_settings_dsc - Test DSC defaults in panel settings init
 * @test: The KUnit test context
 */
static void dm_test_init_panel_settings_dsc(struct kunit *test)
{
	struct dc_panel_config panel_config;
	struct dc_sink *sink;

	memset(&panel_config, 0xFF, sizeof(panel_config));

	sink = kunit_kzalloc(test, sizeof(*sink), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, sink);

	dm_helpers_init_panel_settings(NULL, &panel_config, sink);

	KUNIT_EXPECT_FALSE(test, panel_config.dsc.disable_dsc_edp);
	KUNIT_EXPECT_EQ(test, panel_config.dsc.force_dsc_edp_policy, 0U);
}

/* Tests for dm_helpers_override_panel_settings() */

/**
 * dm_test_override_panel_settings_debug_mask_disables_dsc - Test DSC mask
 * @test: The KUnit test context
 */
static void dm_test_override_panel_settings_debug_mask_disables_dsc(struct kunit *test)
{
	struct dc_context *ctx;
	struct dc_link *link;
	struct dc *dc;
	uint old_debug_mask;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	link = dm_kunit_alloc_link(test);
	ctx->dc = dc;
	link->connector_signal = SIGNAL_TYPE_DISPLAY_PORT;

	old_debug_mask = dm_helpers_get_dc_debug_mask();
	dm_helpers_set_dc_debug_mask(old_debug_mask | DC_DISABLE_DSC);
	dm_helpers_override_panel_settings(ctx, link);
	dm_helpers_set_dc_debug_mask(old_debug_mask);

	KUNIT_EXPECT_TRUE(test, link->panel_config.dsc.disable_dsc_edp);
}

/**
 * dm_test_override_panel_settings_second_edp_disables_psr - Test eDP index 1
 * @test: The KUnit test context
 */
static void dm_test_override_panel_settings_second_edp_disables_psr(struct kunit *test)
{
	struct dc_context *ctx;
	struct dc_link *first_link;
	struct dc_link *second_link;
	struct dc *dc;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, dc);
	first_link = dm_kunit_alloc_link(test);
	second_link = dm_kunit_alloc_link(test);

	ctx->dc = dc;
	dc->link_count = 2;
	dc->links[0] = first_link;
	dc->links[1] = second_link;
	first_link->connector_signal = SIGNAL_TYPE_EDP;
	second_link->connector_signal = SIGNAL_TYPE_EDP;

	dm_helpers_override_panel_settings(ctx, second_link);

	KUNIT_EXPECT_TRUE(test, second_link->panel_config.psr.disable_psr);
	KUNIT_EXPECT_TRUE(test, second_link->panel_config.psr.disallow_psrsu);
	KUNIT_EXPECT_TRUE(test, second_link->panel_config.psr.disallow_replay);
}

/* Tests for fill_dc_mst_payload_table_from_drm() */

/**
 * dm_test_fill_mst_payload_table_enable - Test payload table fill on enable
 * @test: The KUnit test context
 */
static void dm_test_fill_mst_payload_table_enable(struct kunit *test)
{
	struct dc_link *link;
	struct drm_dp_mst_atomic_payload payload = {0};
	struct dc_dp_mst_stream_allocation_table table = {0};

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* Pre-existing allocation in the link table */
	link->mst_stream_alloc_table.stream_count = 1;
	link->mst_stream_alloc_table.stream_allocations[0].vcp_id = 1;
	link->mst_stream_alloc_table.stream_allocations[0].slot_count = 4;

	/* New payload to add */
	payload.vcpi = 2;
	payload.time_slots = 8;

	fill_dc_mst_payload_table_from_drm(link, true, &payload, &table);

	/* Should contain both the pre-existing and new allocation */
	KUNIT_EXPECT_EQ(test, table.stream_count, 2);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[0].vcp_id, 1);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[0].slot_count, 4);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[1].vcp_id, 2);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[1].slot_count, 8);
}

/**
 * dm_test_fill_mst_payload_table_disable - Test payload table fill on disable
 * @test: The KUnit test context
 */
static void dm_test_fill_mst_payload_table_disable(struct kunit *test)
{
	struct dc_link *link;
	struct drm_dp_mst_atomic_payload payload = {0};
	struct dc_dp_mst_stream_allocation_table table = {0};

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* Two existing allocations in the link table */
	link->mst_stream_alloc_table.stream_count = 2;
	link->mst_stream_alloc_table.stream_allocations[0].vcp_id = 1;
	link->mst_stream_alloc_table.stream_allocations[0].slot_count = 4;
	link->mst_stream_alloc_table.stream_allocations[1].vcp_id = 2;
	link->mst_stream_alloc_table.stream_allocations[1].slot_count = 8;

	/* Remove vcp_id 1 */
	payload.vcpi = 1;
	payload.time_slots = 4;

	fill_dc_mst_payload_table_from_drm(link, false, &payload, &table);

	/* Only vcp_id 2 should remain */
	KUNIT_EXPECT_EQ(test, table.stream_count, 1);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[0].vcp_id, 2);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[0].slot_count, 8);
}

/**
 * dm_test_fill_mst_payload_table_empty - Test payload table fill when empty
 * @test: The KUnit test context
 */
static void dm_test_fill_mst_payload_table_empty(struct kunit *test)
{
	struct dc_link *link;
	struct drm_dp_mst_atomic_payload payload = {0};
	struct dc_dp_mst_stream_allocation_table table = {0};

	link = kunit_kzalloc(test, sizeof(*link), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, link);

	/* Enable on an empty table */
	payload.vcpi = 5;
	payload.time_slots = 12;

	fill_dc_mst_payload_table_from_drm(link, true, &payload, &table);

	KUNIT_EXPECT_EQ(test, table.stream_count, 1);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[0].vcp_id, 5);
	KUNIT_EXPECT_EQ(test, table.stream_allocations[0].slot_count, 12);
}

static struct kunit_case amdgpu_dm_helpers_test_cases[] = {
	/* edid_extract_panel_id */
	KUNIT_CASE(dm_test_edid_extract_panel_id_basic),
	KUNIT_CASE(dm_test_edid_extract_panel_id_zeros),
	/* apply_edid_quirks */
	KUNIT_CASE(dm_test_apply_edid_quirks_dpcd_poweroff_delay),
	KUNIT_CASE(dm_test_apply_edid_quirks_disable_fams),
	KUNIT_CASE(dm_test_apply_edid_quirks_remove_sink_ext_caps),
	KUNIT_CASE(dm_test_apply_edid_quirks_disable_colorimetry),
	KUNIT_CASE(dm_test_apply_edid_quirks_skip_phy_ssc),
	KUNIT_CASE(dm_test_apply_edid_quirks_unknown_noop),
	/* dm_helpers_parse_edid_caps */
	KUNIT_CASE(dm_test_parse_edid_caps_null_edid),
	KUNIT_CASE(dm_test_parse_edid_caps_null_caps),
	KUNIT_CASE(dm_test_parse_edid_caps_valid),
	KUNIT_CASE(dm_test_parse_edid_caps_bad_checksum),
	KUNIT_CASE(dm_test_parse_edid_caps_hdmi_frl),
	KUNIT_CASE(dm_test_parse_edid_caps_hdmi_frl_dsc),
	KUNIT_CASE(dm_test_parse_edid_caps_cea_audio),
	KUNIT_CASE(dm_test_parse_edid_caps_cea_no_speaker),
	/* ACPI / VBIOS / local EDID readers */
	KUNIT_CASE(dm_test_probe_acpi_edid_no_companion),
	KUNIT_CASE(dm_test_read_acpi_edid_debug_mask_disabled),
	KUNIT_CASE(dm_test_read_acpi_edid_non_panel_connector),
	KUNIT_CASE(dm_test_read_acpi_edid_force_off),
	KUNIT_CASE(dm_test_read_vbios_edid_non_embedded),
	KUNIT_CASE(dm_test_read_vbios_edid_missing_callback),
	KUNIT_CASE(dm_test_read_vbios_edid_callback_error),
	KUNIT_CASE(dm_test_read_vbios_edid_missing_fake_edid),
	KUNIT_CASE(dm_test_read_vbios_edid_invalid_fake_edid),
	KUNIT_CASE(dm_test_read_vbios_edid_valid),
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
	KUNIT_CASE(dm_test_dp_read_dpcd_success),
	KUNIT_CASE(dm_test_dp_write_dpcd_success),
	/* dm_helpers_execute_fused_io */
	KUNIT_CASE(dm_test_execute_fused_io_null_dmub_srv),
	/* Synaptics RC/FIFO/DSC helpers */
	KUNIT_CASE(dm_test_execute_synaptics_rc_command_write_success),
	KUNIT_CASE(dm_test_execute_synaptics_rc_command_read_success),
	KUNIT_CASE(dm_test_execute_synaptics_rc_command_write_fail),
	KUNIT_CASE(dm_test_apply_synaptics_fifo_reset_wa_full),
	KUNIT_CASE(dm_test_apply_synaptics_fifo_reset_wa_first_fail),
	KUNIT_CASE(dm_test_write_dsc_enable_synaptics_enable_inactive),
	KUNIT_CASE(dm_test_write_dsc_enable_synaptics_enable_active),
	KUNIT_CASE(dm_test_write_dsc_enable_synaptics_disable_inactive),
	KUNIT_CASE(dm_test_write_dsc_enable_synaptics_disable_active),
	KUNIT_CASE(dm_test_write_dsc_enable_synaptics_enable_non_synaptics),
	KUNIT_CASE(dm_test_dp_write_dsc_enable_routes_synaptics),
	/* dm_helpers_dp_mst_start_top_mgr / dm_helpers_dp_mst_stop_top_mgr */
	KUNIT_CASE(dm_test_mst_start_top_mgr_null_priv),
	KUNIT_CASE(dm_test_mst_stop_top_mgr_null_priv),
	KUNIT_CASE(dm_test_mst_start_top_mgr_boot),
	/* dm_helpers_dp_write_hblank_reduction */
	KUNIT_CASE(dm_test_dp_write_hblank_reduction_false),
	/* get_dsc_max_slices */
	KUNIT_CASE(dm_test_get_dsc_max_slices_1_340),
	KUNIT_CASE(dm_test_get_dsc_max_slices_2_340),
	KUNIT_CASE(dm_test_get_dsc_max_slices_4_340),
	KUNIT_CASE(dm_test_get_dsc_max_slices_8_340),
	KUNIT_CASE(dm_test_get_dsc_max_slices_8_400),
	KUNIT_CASE(dm_test_get_dsc_max_slices_12_400),
	KUNIT_CASE(dm_test_get_dsc_max_slices_16_400),
	KUNIT_CASE(dm_test_get_dsc_max_slices_unknown),
	/* dm_helpers_init_panel_settings */
	KUNIT_CASE(dm_test_init_panel_settings_pps),
	KUNIT_CASE(dm_test_init_panel_settings_dsc),
	/* dm_helpers_override_panel_settings */
	KUNIT_CASE(dm_test_override_panel_settings_debug_mask_disables_dsc),
	KUNIT_CASE(dm_test_override_panel_settings_second_edp_disables_psr),
	/* fill_dc_mst_payload_table_from_drm */
	KUNIT_CASE(dm_test_fill_mst_payload_table_enable),
	KUNIT_CASE(dm_test_fill_mst_payload_table_disable),
	KUNIT_CASE(dm_test_fill_mst_payload_table_empty),
	{}
};

static struct kunit_suite amdgpu_dm_helpers_test_suite = {
	.name = "amdgpu_dm_helpers",
	.test_cases = amdgpu_dm_helpers_test_cases,
};

kunit_test_suite(amdgpu_dm_helpers_test_suite);

MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_helpers");
MODULE_LICENSE("Dual MIT/GPL");
