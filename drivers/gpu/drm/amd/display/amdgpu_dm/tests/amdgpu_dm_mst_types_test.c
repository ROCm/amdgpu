// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_mst_types.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>

#include <drm/display/drm_dp.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>

#include "dc.h"
#include "dpcd_defs.h"
#include "dmub_cmd.h"
#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_mst_types.h"

/*
 * Minimal mock DPCD backing store and AUX transfer callback used to exercise
 * the DPCD read paths without real hardware.
 */
static u8 dm_mst_test_dpcd[0x10];

static ssize_t dm_mst_test_aux_transfer(struct drm_dp_aux *aux,
					struct drm_dp_aux_msg *msg)
{
	size_t i;

	switch (msg->request & ~DP_AUX_I2C_MOT) {
	case DP_AUX_NATIVE_READ:
		for (i = 0; i < msg->size; i++)
			((u8 *)msg->buffer)[i] =
				dm_mst_test_dpcd[(msg->address + i) & 0xf];
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;
		return msg->size;
	case DP_AUX_NATIVE_WRITE:
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;
		return msg->size;
	default:
		return -EINVAL;
	}
}

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

/**
 * dm_mst_test_needs_dsc_aux_workaround_zero_sink_count - Test workaround skipped for zero sinks
 * @test: KUnit test context
 *
 * Verify that needs_dsc_aux_workaround() returns false when the sink
 * count is zero, even if device ID and DPCD rev match.
 */
static void dm_mst_test_needs_dsc_aux_workaround_zero_sink_count(struct kunit *test)
{
	struct dc_link link = {0};

	link.dpcd_caps.branch_dev_id = DP_BRANCH_DEVICE_ID_90CC24;
	link.dpcd_caps.dpcd_rev.raw = DPCD_REV_14;
	link.dpcd_caps.sink_count.bits.SINK_COUNT = 0;

	KUNIT_EXPECT_FALSE(test, needs_dsc_aux_workaround(&link));
}

/* Tests for dm_mst_get_pbn_divider */

/**
 * dm_mst_test_pbn_divider_null_link - Test pbn_divider with NULL link
 * @test: KUnit test context
 *
 * Verify that dm_mst_get_pbn_divider() returns 0 when passed a NULL
 * link pointer without crashing.
 */
static void dm_mst_test_pbn_divider_null_link(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, dm_mst_get_pbn_divider(NULL), 0U);
}

/* Tests for amdgpu_dm_mst_reset_mst_connector_setting */

/**
 * dm_mst_test_reset_connector_setting - Test MST connector setting reset
 * @test: KUnit test context
 *
 * Verify that amdgpu_dm_mst_reset_mst_connector_setting() clears the cached
 * EDID, DSC AUX, passthrough AUX, local bandwidth, and VC PBN state.
 */
static void dm_mst_test_reset_connector_setting(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct drm_dp_mst_port *port;

	aconnector = kunit_kzalloc(test, sizeof(*aconnector), GFP_KERNEL);
	port = kunit_kzalloc(test, sizeof(*port), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, aconnector);
	KUNIT_ASSERT_NOT_NULL(test, port);

	aconnector->drm_edid = (const struct drm_edid *)test;
	aconnector->dsc_aux = (struct drm_dp_aux *)test;
	aconnector->mst_output_port = port;
	aconnector->mst_output_port->passthrough_aux = (struct drm_dp_aux *)test;
	aconnector->mst_local_bw = 12345;
	aconnector->vc_full_pbn = 678;

	amdgpu_dm_mst_reset_mst_connector_setting(aconnector);

	KUNIT_EXPECT_TRUE(test, aconnector->drm_edid == NULL);
	KUNIT_EXPECT_TRUE(test, aconnector->dsc_aux == NULL);
	KUNIT_EXPECT_TRUE(test, aconnector->mst_output_port->passthrough_aux == NULL);
	KUNIT_EXPECT_EQ(test, aconnector->mst_local_bw, 0U);
	KUNIT_EXPECT_EQ(test, aconnector->vc_full_pbn, 0U);
}

/* Tests for retrieve_downstream_port_device */

/**
 * dm_mst_test_retrieve_downstream_no_aux - Test retrieval bails out without AUX
 * @test: KUnit test context
 *
 * Verify that retrieve_downstream_port_device() returns false when the
 * connector has no DSC AUX channel and therefore cannot read DPCD.
 */
static void dm_mst_test_retrieve_downstream_no_aux(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;

	aconnector = kunit_kzalloc(test, sizeof(*aconnector), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, aconnector);

	aconnector->dsc_aux = NULL;

	KUNIT_EXPECT_FALSE(test, retrieve_downstream_port_device(aconnector));
}

/**
 * dm_mst_test_retrieve_downstream_present - Test retrieval parses DPCD 0x05
 * @test: KUnit test context
 *
 * Verify that retrieve_downstream_port_device() reads DP_DOWNSTREAMPORT_PRESENT
 * over a mock AUX channel and caches the parsed downstream port fields.
 */
static void dm_mst_test_retrieve_downstream_present(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct drm_dp_aux *aux;

	aconnector = kunit_kzalloc(test, sizeof(*aconnector), GFP_KERNEL);
	aux = kunit_kzalloc(test, sizeof(*aux), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, aconnector);
	KUNIT_ASSERT_NOT_NULL(test, aux);

	memset(dm_mst_test_dpcd, 0, sizeof(dm_mst_test_dpcd));
	/* PORT_PRESENT = 1, PORT_TYPE = 2 (0b101) */
	dm_mst_test_dpcd[DP_DOWNSTREAMPORT_PRESENT] = 0x05;

	aux->name = "dm_mst_test_aux";
	aux->transfer = dm_mst_test_aux_transfer;
	drm_dp_aux_init(aux);
	drm_dp_dpcd_set_probe(aux, false);
	aconnector->dsc_aux = aux;

	KUNIT_EXPECT_TRUE(test, retrieve_downstream_port_device(aconnector));
	KUNIT_EXPECT_EQ(test,
			(int)aconnector->mst_downstream_port_present.fields.PORT_PRESENT, 1);
	KUNIT_EXPECT_EQ(test,
			(int)aconnector->mst_downstream_port_present.fields.PORT_TYPE, 2);
}

/* Tests for retrieve_branch_specific_data */

/**
 * dm_mst_test_retrieve_branch_no_parent - Test branch lookup needs a parent port
 * @test: KUnit test context
 *
 * Verify that retrieve_branch_specific_data() returns false when the MST
 * output port has no parent branch device to query.
 */
static void dm_mst_test_retrieve_branch_no_parent(struct kunit *test)
{
	struct amdgpu_dm_connector *aconnector;
	struct drm_dp_mst_port *port;

	aconnector = kunit_kzalloc(test, sizeof(*aconnector), GFP_KERNEL);
	port = kunit_kzalloc(test, sizeof(*port), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, aconnector);
	KUNIT_ASSERT_NOT_NULL(test, port);

	port->parent = NULL;
	aconnector->mst_output_port = port;

	KUNIT_EXPECT_FALSE(test, retrieve_branch_specific_data(aconnector));
}

/**
 * dm_mst_test_aux_result_success - AUX_RET_SUCCESS preserves the input result.
 * @test: KUnit test context.
 *
 * On success the original (negative) transfer result must be returned unchanged.
 */
static void dm_mst_test_aux_result_success(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-5, AUX_RET_SUCCESS), (ssize_t)-5);
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(3, AUX_RET_SUCCESS), (ssize_t)3);
}

/**
 * dm_mst_test_aux_result_eio - HPD/unknown/protocol errors map to -EIO.
 * @test: KUnit test context.
 *
 * AUX_RET_ERROR_HPD_DISCON, AUX_RET_ERROR_UNKNOWN,
 * AUX_RET_ERROR_INVALID_OPERATION and AUX_RET_ERROR_PROTOCOL_ERROR all map to -EIO.
 */
static void dm_mst_test_aux_result_eio(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-1, AUX_RET_ERROR_HPD_DISCON),
			(ssize_t)-EIO);
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-1, AUX_RET_ERROR_UNKNOWN),
			(ssize_t)-EIO);
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-1, AUX_RET_ERROR_INVALID_OPERATION),
			(ssize_t)-EIO);
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-1, AUX_RET_ERROR_PROTOCOL_ERROR),
			(ssize_t)-EIO);
}

/**
 * dm_mst_test_aux_result_ebusy - invalid reply / engine acquire map to -EBUSY.
 * @test: KUnit test context.
 *
 * AUX_RET_ERROR_INVALID_REPLY and AUX_RET_ERROR_ENGINE_ACQUIRE map to -EBUSY.
 */
static void dm_mst_test_aux_result_ebusy(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-1, AUX_RET_ERROR_INVALID_REPLY),
			(ssize_t)-EBUSY);
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-1, AUX_RET_ERROR_ENGINE_ACQUIRE),
			(ssize_t)-EBUSY);
}

/**
 * dm_mst_test_aux_result_timeout - AUX_RET_ERROR_TIMEOUT maps to -ETIMEDOUT.
 * @test: KUnit test context.
 */
static void dm_mst_test_aux_result_timeout(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, dm_dp_aux_transfer_result(-1, AUX_RET_ERROR_TIMEOUT),
			(ssize_t)-ETIMEDOUT);
}

/**
 * dm_mst_test_fill_payload_flags_native_write - native write request decode.
 * @test: KUnit test context.
 *
 * DP_AUX_NATIVE_WRITE clears i2c_over_aux and sets write; no I2C bits set.
 */
static void dm_mst_test_fill_payload_flags_native_write(struct kunit *test)
{
	struct aux_payload payload = { 0 };

	dm_dp_aux_fill_payload_flags(DP_AUX_NATIVE_WRITE, &payload);

	KUNIT_EXPECT_FALSE(test, payload.i2c_over_aux);
	KUNIT_EXPECT_TRUE(test, payload.write);
	KUNIT_EXPECT_FALSE(test, payload.mot);
	KUNIT_EXPECT_FALSE(test, payload.write_status_update);
}

/**
 * dm_mst_test_fill_payload_flags_native_read - native read request decode.
 * @test: KUnit test context.
 *
 * DP_AUX_NATIVE_READ keeps i2c_over_aux clear; the I2C_READ bit clears write.
 */
static void dm_mst_test_fill_payload_flags_native_read(struct kunit *test)
{
	struct aux_payload payload = { 0 };

	dm_dp_aux_fill_payload_flags(DP_AUX_NATIVE_READ, &payload);

	KUNIT_EXPECT_FALSE(test, payload.i2c_over_aux);
	KUNIT_EXPECT_FALSE(test, payload.write);
	KUNIT_EXPECT_FALSE(test, payload.mot);
}

/**
 * dm_mst_test_fill_payload_flags_i2c_read_mot - I2C read with MOT request decode.
 * @test: KUnit test context.
 *
 * DP_AUX_I2C_READ sets i2c_over_aux and clears write; DP_AUX_I2C_MOT sets mot.
 */
static void dm_mst_test_fill_payload_flags_i2c_read_mot(struct kunit *test)
{
	struct aux_payload payload = { 0 };

	dm_dp_aux_fill_payload_flags(DP_AUX_I2C_READ | DP_AUX_I2C_MOT, &payload);

	KUNIT_EXPECT_TRUE(test, payload.i2c_over_aux);
	KUNIT_EXPECT_FALSE(test, payload.write);
	KUNIT_EXPECT_TRUE(test, payload.mot);
}

/**
 * dm_mst_test_fill_payload_flags_write_status - write status update decode.
 * @test: KUnit test context.
 *
 * DP_AUX_I2C_WRITE_STATUS_UPDATE sets write_status_update.
 */
static void dm_mst_test_fill_payload_flags_write_status(struct kunit *test)
{
	struct aux_payload payload = { 0 };

	dm_dp_aux_fill_payload_flags(DP_AUX_I2C_WRITE | DP_AUX_I2C_WRITE_STATUS_UPDATE,
				     &payload);

	KUNIT_EXPECT_TRUE(test, payload.i2c_over_aux);
	KUNIT_EXPECT_TRUE(test, payload.write_status_update);
}

/**
 * dm_mst_test_msg_ready_mask - ESI mask selection per message-ready type.
 * @test: KUnit test context.
 *
 * DOWN_REP and UP_REQ each select their single bit; other types select both.
 */
static void dm_mst_test_msg_ready_mask(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, dm_mst_msg_ready_mask(DOWN_REP_MSG_RDY_EVENT),
			(u8)DP_DOWN_REP_MSG_RDY);
	KUNIT_EXPECT_EQ(test, dm_mst_msg_ready_mask(UP_REQ_MSG_RDY_EVENT),
			(u8)DP_UP_REQ_MSG_RDY);
	KUNIT_EXPECT_EQ(test, dm_mst_msg_ready_mask(DOWN_OR_UP_MSG_RDY_EVENT),
			(u8)(DP_DOWN_REP_MSG_RDY | DP_UP_REQ_MSG_RDY));
	KUNIT_EXPECT_EQ(test, dm_mst_msg_ready_mask(NONE_MSG_RDY_EVENT),
			(u8)(DP_DOWN_REP_MSG_RDY | DP_UP_REQ_MSG_RDY));
}

/**
 * dm_mst_test_select_esi_dpcd_legacy - pre-1.2 DPCD ESI address/length.
 * @test: KUnit test context.
 *
 * For DPCD rev < 0x12 the legacy DP_SINK_COUNT address/length pair is selected.
 */
static void dm_mst_test_select_esi_dpcd_legacy(struct kunit *test)
{
	int dpcd_addr = -1;
	u8 dpcd_bytes_to_read = 0;

	dm_mst_select_esi_dpcd(0x11, &dpcd_addr, &dpcd_bytes_to_read);

	KUNIT_EXPECT_EQ(test, dpcd_addr, DP_SINK_COUNT);
	KUNIT_EXPECT_EQ(test, (int)dpcd_bytes_to_read,
			(int)(DP_LANE0_1_STATUS - DP_SINK_COUNT));
}

/**
 * dm_mst_test_select_esi_dpcd_esi - 1.2+ DPCD ESI address/length.
 * @test: KUnit test context.
 *
 * For DPCD rev >= 0x12 the ESI DP_SINK_COUNT_ESI address/length pair is selected.
 */
static void dm_mst_test_select_esi_dpcd_esi(struct kunit *test)
{
	int dpcd_addr = -1;
	u8 dpcd_bytes_to_read = 0;

	dm_mst_select_esi_dpcd(0x14, &dpcd_addr, &dpcd_bytes_to_read);

	KUNIT_EXPECT_EQ(test, dpcd_addr, DP_SINK_COUNT_ESI);
	KUNIT_EXPECT_EQ(test, (int)dpcd_bytes_to_read,
			(int)(DP_PSR_ERROR_STATUS - DP_SINK_COUNT_ESI));
}

static struct kunit_case dm_mst_types_test_cases[] = {
	/* needs_dsc_aux_workaround tests */
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_match),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_rev12),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_wrong_dev_id),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_wrong_rev),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_low_sink_count),
	KUNIT_CASE(dm_mst_test_needs_dsc_aux_workaround_zero_sink_count),
	/* dm_mst_get_pbn_divider tests */
	KUNIT_CASE(dm_mst_test_pbn_divider_null_link),
	/* amdgpu_dm_mst_reset_mst_connector_setting tests */
	KUNIT_CASE(dm_mst_test_reset_connector_setting),
	/* retrieve_downstream_port_device tests */
	KUNIT_CASE(dm_mst_test_retrieve_downstream_no_aux),
	KUNIT_CASE(dm_mst_test_retrieve_downstream_present),
	/* retrieve_branch_specific_data tests */
	KUNIT_CASE(dm_mst_test_retrieve_branch_no_parent),
	/* dm_dp_aux_transfer_result tests */
	KUNIT_CASE(dm_mst_test_aux_result_success),
	KUNIT_CASE(dm_mst_test_aux_result_eio),
	KUNIT_CASE(dm_mst_test_aux_result_ebusy),
	KUNIT_CASE(dm_mst_test_aux_result_timeout),
	/* dm_dp_aux_fill_payload_flags tests */
	KUNIT_CASE(dm_mst_test_fill_payload_flags_native_write),
	KUNIT_CASE(dm_mst_test_fill_payload_flags_native_read),
	KUNIT_CASE(dm_mst_test_fill_payload_flags_i2c_read_mot),
	KUNIT_CASE(dm_mst_test_fill_payload_flags_write_status),
	/* dm_mst_msg_ready_mask tests */
	KUNIT_CASE(dm_mst_test_msg_ready_mask),
	/* dm_mst_select_esi_dpcd tests */
	KUNIT_CASE(dm_mst_test_select_esi_dpcd_legacy),
	KUNIT_CASE(dm_mst_test_select_esi_dpcd_esi),
	{}
};

static struct kunit_suite dm_mst_types_test_suite = {
	.name = "amdgpu_dm_mst_types",
	.test_cases = dm_mst_types_test_cases,
};

kunit_test_suite(dm_mst_types_test_suite);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_mst_types");
