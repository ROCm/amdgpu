// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_irq.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <drm/drm_kunit_helpers.h>

#include "dc.h"
#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_irq.h"
#include "dmub/dmub_srv.h"

static void dm_test_irq_handler(void *arg)
{
}

static void dm_test_irq_handler_alt(void *arg)
{
}

static void dm_test_crtc_list_del(void *data)
{
	struct amdgpu_crtc *acrtc = data;

	list_del_init(&acrtc->base.head);
}

/* Tests for amdgpu_dm_hpd_to_dal_irq_source() */

/**
 * dm_test_hpd_to_dal_irq_source_hpd1 - Test Hpd to dal irq source hpd1
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_hpd1(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(AMDGPU_HPD_1),
			(int)DC_IRQ_SOURCE_HPD1);
}

/**
 * dm_test_hpd_to_dal_irq_source_hpd2 - Test Hpd to dal irq source hpd2
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_hpd2(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(AMDGPU_HPD_2),
			(int)DC_IRQ_SOURCE_HPD2);
}

/**
 * dm_test_hpd_to_dal_irq_source_hpd3 - Test Hpd to dal irq source hpd3
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_hpd3(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(AMDGPU_HPD_3),
			(int)DC_IRQ_SOURCE_HPD3);
}

/**
 * dm_test_hpd_to_dal_irq_source_hpd4 - Test Hpd to dal irq source hpd4
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_hpd4(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(AMDGPU_HPD_4),
			(int)DC_IRQ_SOURCE_HPD4);
}

/**
 * dm_test_hpd_to_dal_irq_source_hpd5 - Test Hpd to dal irq source hpd5
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_hpd5(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(AMDGPU_HPD_5),
			(int)DC_IRQ_SOURCE_HPD5);
}

/**
 * dm_test_hpd_to_dal_irq_source_hpd6 - Test Hpd to dal irq source hpd6
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_hpd6(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(AMDGPU_HPD_6),
			(int)DC_IRQ_SOURCE_HPD6);
}

/**
 * dm_test_hpd_to_dal_irq_source_invalid - Test Hpd to dal irq source invalid
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_invalid(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(AMDGPU_HPD_NONE),
			(int)DC_IRQ_SOURCE_INVALID);
}

/**
 * dm_test_hpd_to_dal_irq_source_out_of_range - Test Hpd to dal irq source out of range
 * @test: The KUnit test context
 */
static void dm_test_hpd_to_dal_irq_source_out_of_range(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)amdgpu_dm_hpd_to_dal_irq_source(99),
			(int)DC_IRQ_SOURCE_INVALID);
}

/* Tests for are_sinks_equal() */

/**
 * dm_test_are_sinks_equal_both_null - Test Are sinks equal both null
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_both_null(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, are_sinks_equal(NULL, NULL));
}

/**
 * dm_test_are_sinks_equal_first_null - Test Are sinks equal first null
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_first_null(struct kunit *test)
{
	struct dc_sink *sink2;

	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	KUNIT_EXPECT_FALSE(test, are_sinks_equal(NULL, sink2));
}

/**
 * dm_test_are_sinks_equal_second_null - Test Are sinks equal second null
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_second_null(struct kunit *test)
{
	struct dc_sink *sink1;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);

	KUNIT_EXPECT_FALSE(test, are_sinks_equal(sink1, NULL));
}

/**
 * dm_test_are_sinks_equal_different_signal - Test Are sinks equal different signal
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_different_signal(struct kunit *test)
{
	struct dc_sink *sink1, *sink2;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);
	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	sink1->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink2->sink_signal = SIGNAL_TYPE_DISPLAY_PORT;

	KUNIT_EXPECT_FALSE(test, are_sinks_equal(sink1, sink2));
}

/**
 * dm_test_are_sinks_equal_different_edid_length - Test Are sinks equal different edid length
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_different_edid_length(struct kunit *test)
{
	struct dc_sink *sink1, *sink2;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);
	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	sink1->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink2->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink1->dc_edid.length = 128;
	sink2->dc_edid.length = 256;

	KUNIT_EXPECT_FALSE(test, are_sinks_equal(sink1, sink2));
}

/**
 * dm_test_are_sinks_equal_different_edid_data - Test Are sinks equal different edid data
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_different_edid_data(struct kunit *test)
{
	struct dc_sink *sink1, *sink2;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);
	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	sink1->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink2->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink1->dc_edid.length = 4;
	sink2->dc_edid.length = 4;
	memset(sink1->dc_edid.raw_edid, 0xAA, 4);
	memset(sink2->dc_edid.raw_edid, 0xBB, 4);

	KUNIT_EXPECT_FALSE(test, are_sinks_equal(sink1, sink2));
}

/**
 * dm_test_are_sinks_equal_identical - Test Are sinks equal identical
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_identical(struct kunit *test)
{
	struct dc_sink *sink1, *sink2;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);
	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	sink1->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink2->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink1->dc_edid.length = 4;
	sink2->dc_edid.length = 4;
	memset(sink1->dc_edid.raw_edid, 0xAA, 4);
	memset(sink2->dc_edid.raw_edid, 0xAA, 4);

	KUNIT_EXPECT_TRUE(test, are_sinks_equal(sink1, sink2));
}

/**
 * dm_test_are_sinks_equal_zero_length - Test Are sinks equal zero length
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_zero_length(struct kunit *test)
{
	struct dc_sink *sink1, *sink2;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);
	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	sink1->sink_signal = SIGNAL_TYPE_DISPLAY_PORT;
	sink2->sink_signal = SIGNAL_TYPE_DISPLAY_PORT;
	sink1->dc_edid.length = 0;
	sink2->dc_edid.length = 0;

	KUNIT_EXPECT_TRUE(test, are_sinks_equal(sink1, sink2));
}

/**
 * dm_test_are_sinks_equal_full_edid_identical - Test Are sinks equal full edid identical
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_full_edid_identical(struct kunit *test)
{
	struct dc_sink *sink1, *sink2;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);
	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	sink1->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink2->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink1->dc_edid.length = 128;
	sink2->dc_edid.length = 128;
	memset(sink1->dc_edid.raw_edid, 0x5A, 128);
	memset(sink2->dc_edid.raw_edid, 0x5A, 128);

	KUNIT_EXPECT_TRUE(test, are_sinks_equal(sink1, sink2));
}

/**
 * dm_test_are_sinks_equal_full_edid_last_byte_differs - Test Are sinks equal last byte differs
 * @test: The KUnit test context
 */
static void dm_test_are_sinks_equal_full_edid_last_byte_differs(struct kunit *test)
{
	struct dc_sink *sink1, *sink2;

	sink1 = kunit_kzalloc(test, sizeof(*sink1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink1);
	sink2 = kunit_kzalloc(test, sizeof(*sink2), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sink2);

	sink1->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink2->sink_signal = SIGNAL_TYPE_HDMI_TYPE_A;
	sink1->dc_edid.length = 128;
	sink2->dc_edid.length = 128;
	memset(sink1->dc_edid.raw_edid, 0x5A, 128);
	memset(sink2->dc_edid.raw_edid, 0x5A, 128);
	sink2->dc_edid.raw_edid[127] = 0x5B;

	KUNIT_EXPECT_FALSE(test, are_sinks_equal(sink1, sink2));
}

/* Tests for dmub_notification_type_str() */

/**
 * dm_test_notification_str_no_data - Test Notification str no data
 * @test: The KUnit test context
 */
static void dm_test_notification_str_no_data(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_NO_DATA), "NO_DATA");
}

/**
 * dm_test_notification_str_aux_reply - Test Notification str aux reply
 * @test: The KUnit test context
 */
static void dm_test_notification_str_aux_reply(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_AUX_REPLY), "AUX_REPLY");
}

/**
 * dm_test_notification_str_hpd - Test Notification str hpd
 * @test: The KUnit test context
 */
static void dm_test_notification_str_hpd(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_HPD), "HPD");
}

/**
 * dm_test_notification_str_hpd_irq - Test Notification str hpd irq
 * @test: The KUnit test context
 */
static void dm_test_notification_str_hpd_irq(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_HPD_IRQ), "HPD_IRQ");
}

/**
 * dm_test_notification_str_set_config - Test Notification str set config
 * @test: The KUnit test context
 */
static void dm_test_notification_str_set_config(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_SET_CONFIG_REPLY),
			   "SET_CONFIG_REPLY");
}

/**
 * dm_test_notification_str_dpia - Test Notification str dpia
 * @test: The KUnit test context
 */
static void dm_test_notification_str_dpia(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_DPIA_NOTIFICATION),
			   "DPIA_NOTIFICATION");
}

/**
 * dm_test_notification_str_hpd_sense - Test Notification str hpd sense
 * @test: The KUnit test context
 */
static void dm_test_notification_str_hpd_sense(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_HPD_SENSE_NOTIFY),
			   "HPD_SENSE_NOTIFY");
}

/**
 * dm_test_notification_str_fused_io - Test Notification str fused io
 * @test: The KUnit test context
 */
static void dm_test_notification_str_fused_io(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_FUSED_IO),
			   "FUSED_IO");
}

/**
 * dm_test_notification_str_unknown - Test Notification str unknown
 * @test: The KUnit test context
 */
static void dm_test_notification_str_unknown(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, dmub_notification_type_str(DMUB_NOTIFICATION_MAX), "<unknown>");
}

/* Tests for amdgpu_dm_irq_init() */

/**
 * dm_test_irq_init_initializes_lists - Test irq init initializes list heads
 * @test: The KUnit test context
 */
static void dm_test_irq_init_initializes_lists(struct kunit *test)
{
	struct amdgpu_device *adev;
	int src;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	KUNIT_EXPECT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	for (src = 0; src < DAL_IRQ_SOURCES_NUMBER; src++) {
		KUNIT_EXPECT_TRUE(test,
				  list_empty(&adev->dm.irq_handler_list_low_tab[src]));
		KUNIT_EXPECT_TRUE(test,
				  list_empty(&adev->dm.irq_handler_list_high_tab[src]));
	}
}

/* Tests for amdgpu_dm_irq_register_interrupt() */

/**
 * dm_test_irq_register_rejects_null_params - Test register rejects null params
 * @test: The KUnit test context
 */
static void dm_test_irq_register_rejects_null_params(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;

	KUNIT_EXPECT_NULL(test,
		amdgpu_dm_irq_register_interrupt(adev, NULL,
						   dm_test_irq_handler, NULL));
	KUNIT_EXPECT_NULL(test,
		amdgpu_dm_irq_register_interrupt(adev, &int_params, NULL, NULL));
}

/**
 * dm_test_irq_register_rejects_invalid_context - Test register rejects context
 * @test: The KUnit test context
 */
static void dm_test_irq_register_rejects_invalid_context(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	int_params.int_context = INTERRUPT_CONTEXT_NUMBER;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;

	KUNIT_EXPECT_NULL(test,
		amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler, NULL));
}

/**
 * dm_test_irq_register_rejects_invalid_source - Test register rejects source
 * @test: The KUnit test context
 */
static void dm_test_irq_register_rejects_invalid_source(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_INVALID;

	KUNIT_EXPECT_NULL(test,
		amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler, NULL));
}

/**
 * dm_test_irq_register_adds_low_context_handler - Test register adds low handler
 * @test: The KUnit test context
 */
static void dm_test_irq_register_adds_low_context_handler(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;

	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);
	KUNIT_EXPECT_FALSE(test,
			   list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD1]));
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD1]));

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
						dm_test_irq_handler);
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD1]));
}

/**
 * dm_test_irq_register_adds_high_context_handler - Test register adds high handler
 * @test: The KUnit test context
 */
static void dm_test_irq_register_adds_high_context_handler(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_HIGH_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD2;

	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);
	KUNIT_EXPECT_FALSE(test,
			   list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD2]));
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD2]));

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD2,
						dm_test_irq_handler);
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD2]));
}

/**
 * dm_test_irq_register_multiple_handlers - Test register keeps multiple handlers
 * @test: The KUnit test context
 */
static void dm_test_irq_register_multiple_handlers(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	struct list_head *hnd_list;
	void *handler1, *handler2;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;

	handler1 = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler1);
	handler2 = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler_alt, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler2);

	hnd_list = &adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD1];
	KUNIT_EXPECT_EQ(test, list_count_nodes(hnd_list), 2);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
						dm_test_irq_handler);
	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
						dm_test_irq_handler_alt);
	KUNIT_EXPECT_TRUE(test, list_empty(hnd_list));
}

/**
 * dm_test_irq_register_separate_contexts - Test register same source in two contexts
 * @test: The KUnit test context
 */
static void dm_test_irq_register_separate_contexts(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.irq_source = DC_IRQ_SOURCE_HPD5;

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	int_params.int_context = INTERRUPT_HIGH_IRQ_CONTEXT;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	KUNIT_EXPECT_FALSE(test,
			   list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD5]));
	KUNIT_EXPECT_FALSE(test,
			   list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD5]));

	/*
	 * A single unregister call stops at the first context where the handler
	 * is found (low context), leaving the high context handler in place.
	 */
	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD5,
						dm_test_irq_handler);

	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD5]));
	KUNIT_EXPECT_FALSE(test,
			   list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD5]));

	/* A second call removes the remaining high context handler. */
	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD5,
						dm_test_irq_handler);

	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD5]));
}

/* Tests for amdgpu_dm_irq_unregister_interrupt() */

/**
 * dm_test_irq_unregister_rejects_invalid_source - Test unregister rejects source
 * @test: The KUnit test context
 */
static void dm_test_irq_unregister_rejects_invalid_source(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_INVALID,
						dm_test_irq_handler);

	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD1]));
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD1]));
}

/**
 * dm_test_irq_unregister_rejects_null_handler - Test unregister rejects handler
 * @test: The KUnit test context
 */
static void dm_test_irq_unregister_rejects_null_handler(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
						DAL_INVALID_IRQ_HANDLER_IDX);

	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD1]));
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD1]));
}

/**
 * dm_test_irq_unregister_handler_not_found - Test unregister keeps unmatched handler
 * @test: The KUnit test context
 */
static void dm_test_irq_unregister_handler_not_found(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	/* Unregister a handler that was never registered for this source. */
	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
						dm_test_irq_handler_alt);

	/* The originally registered handler must still be present. */
	KUNIT_EXPECT_FALSE(test,
			   list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD1]));

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
						dm_test_irq_handler);
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD1]));
}

/* Tests for amdgpu_dm_irq_fini() */

/**
 * dm_test_irq_fini_removes_registered_handlers - Test fini removes handlers
 * @test: The KUnit test context
 */
static void dm_test_irq_fini_removes_registered_handlers(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD3;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	int_params.int_context = INTERRUPT_HIGH_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD4;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						    dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	amdgpu_dm_irq_fini(adev);

	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_low_tab[DC_IRQ_SOURCE_HPD3]));
	KUNIT_EXPECT_TRUE(test,
			  list_empty(&adev->dm.irq_handler_list_high_tab[DC_IRQ_SOURCE_HPD4]));
}

/**
 * dm_test_irq_fini_on_empty_tables - Test fini on tables with no handlers
 * @test: The KUnit test context
 */
static void dm_test_irq_fini_on_empty_tables(struct kunit *test)
{
	struct amdgpu_device *adev;
	int src;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	amdgpu_dm_irq_fini(adev);

	for (src = 0; src < DAL_IRQ_SOURCES_NUMBER; src++) {
		KUNIT_EXPECT_TRUE(test,
				  list_empty(&adev->dm.irq_handler_list_low_tab[src]));
		KUNIT_EXPECT_TRUE(test,
				  list_empty(&adev->dm.irq_handler_list_high_tab[src]));
	}
}

/* Tests for amdgpu_dm_get_crtc_by_otg_inst() */

/**
 * dm_test_get_crtc_by_otg_inst_returns_match - Test CRTC lookup by OTG instance
 * @test: The KUnit test context
 */
static void dm_test_get_crtc_by_otg_inst_returns_match(struct kunit *test)
{
	struct amdgpu_crtc *acrtc_a, *acrtc_b;
	struct amdgpu_device *adev;
	struct drm_device *drm;
	struct device *dev;

	dev = drm_kunit_helper_alloc_device(test);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	drm = __drm_kunit_helper_alloc_drm_device(test, dev,
						   sizeof(*adev),
						   offsetof(struct amdgpu_device, ddev),
						   DRIVER_MODESET);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drm);
	adev = drm_to_adev(drm);

	acrtc_a = kunit_kzalloc(test, sizeof(*acrtc_a), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc_a);
	acrtc_b = kunit_kzalloc(test, sizeof(*acrtc_b), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc_b);

	INIT_LIST_HEAD(&acrtc_a->base.head);
	INIT_LIST_HEAD(&acrtc_b->base.head);
	acrtc_a->otg_inst = 1;
	acrtc_b->otg_inst = 3;

	list_add_tail(&acrtc_a->base.head, &drm->mode_config.crtc_list);
	KUNIT_ASSERT_EQ(test, kunit_add_action_or_reset(test, dm_test_crtc_list_del,
							acrtc_a), 0);
	list_add_tail(&acrtc_b->base.head, &drm->mode_config.crtc_list);
	KUNIT_ASSERT_EQ(test, kunit_add_action_or_reset(test, dm_test_crtc_list_del,
							acrtc_b), 0);

	KUNIT_EXPECT_PTR_EQ(test, amdgpu_dm_get_crtc_by_otg_inst(adev, 3), acrtc_b);
}

/**
 * dm_test_get_crtc_by_otg_inst_returns_null - Test CRTC lookup misses unknown OTG
 * @test: The KUnit test context
 */
static void dm_test_get_crtc_by_otg_inst_returns_null(struct kunit *test)
{
	struct amdgpu_crtc *acrtc;
	struct amdgpu_device *adev;
	struct drm_device *drm;
	struct device *dev;

	dev = drm_kunit_helper_alloc_device(test);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	drm = __drm_kunit_helper_alloc_drm_device(test, dev,
						   sizeof(*adev),
						   offsetof(struct amdgpu_device, ddev),
						   DRIVER_MODESET);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drm);
	adev = drm_to_adev(drm);

	acrtc = kunit_kzalloc(test, sizeof(*acrtc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc);

	INIT_LIST_HEAD(&acrtc->base.head);
	acrtc->otg_inst = 2;

	list_add_tail(&acrtc->base.head, &drm->mode_config.crtc_list);
	KUNIT_ASSERT_EQ(test, kunit_add_action_or_reset(test, dm_test_crtc_list_del,
							acrtc), 0);

	KUNIT_EXPECT_NULL(test, amdgpu_dm_get_crtc_by_otg_inst(adev, 5));
}

/**
 * dm_test_get_crtc_by_otg_inst_empty_list - Test CRTC lookup on empty CRTC list
 * @test: The KUnit test context
 */
static void dm_test_get_crtc_by_otg_inst_empty_list(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct drm_device *drm;
	struct device *dev;

	dev = drm_kunit_helper_alloc_device(test);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	drm = __drm_kunit_helper_alloc_drm_device(test, dev,
						   sizeof(*adev),
						   offsetof(struct amdgpu_device, ddev),
						   DRIVER_MODESET);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drm);
	adev = drm_to_adev(drm);

	KUNIT_EXPECT_NULL(test, amdgpu_dm_get_crtc_by_otg_inst(adev, 0));
}

static struct kunit_case amdgpu_dm_irq_tests[] = {
	/* amdgpu_dm_hpd_to_dal_irq_source */
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_hpd1),
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_hpd2),
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_hpd3),
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_hpd4),
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_hpd5),
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_hpd6),
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_invalid),
	KUNIT_CASE(dm_test_hpd_to_dal_irq_source_out_of_range),
	/* are_sinks_equal */
	KUNIT_CASE(dm_test_are_sinks_equal_both_null),
	KUNIT_CASE(dm_test_are_sinks_equal_first_null),
	KUNIT_CASE(dm_test_are_sinks_equal_second_null),
	KUNIT_CASE(dm_test_are_sinks_equal_different_signal),
	KUNIT_CASE(dm_test_are_sinks_equal_different_edid_length),
	KUNIT_CASE(dm_test_are_sinks_equal_different_edid_data),
	KUNIT_CASE(dm_test_are_sinks_equal_identical),
	KUNIT_CASE(dm_test_are_sinks_equal_zero_length),
	KUNIT_CASE(dm_test_are_sinks_equal_full_edid_identical),
	KUNIT_CASE(dm_test_are_sinks_equal_full_edid_last_byte_differs),
	/* dmub_notification_type_str */
	KUNIT_CASE(dm_test_notification_str_no_data),
	KUNIT_CASE(dm_test_notification_str_aux_reply),
	KUNIT_CASE(dm_test_notification_str_hpd),
	KUNIT_CASE(dm_test_notification_str_hpd_irq),
	KUNIT_CASE(dm_test_notification_str_set_config),
	KUNIT_CASE(dm_test_notification_str_dpia),
	KUNIT_CASE(dm_test_notification_str_hpd_sense),
	KUNIT_CASE(dm_test_notification_str_fused_io),
	KUNIT_CASE(dm_test_notification_str_unknown),
	/* amdgpu_dm_irq_init */
	KUNIT_CASE(dm_test_irq_init_initializes_lists),
	/* amdgpu_dm_irq_register_interrupt */
	KUNIT_CASE(dm_test_irq_register_rejects_null_params),
	KUNIT_CASE(dm_test_irq_register_rejects_invalid_context),
	KUNIT_CASE(dm_test_irq_register_rejects_invalid_source),
	KUNIT_CASE(dm_test_irq_register_adds_low_context_handler),
	KUNIT_CASE(dm_test_irq_register_adds_high_context_handler),
	KUNIT_CASE(dm_test_irq_register_multiple_handlers),
	KUNIT_CASE(dm_test_irq_register_separate_contexts),
	/* amdgpu_dm_irq_unregister_interrupt */
	KUNIT_CASE(dm_test_irq_unregister_rejects_invalid_source),
	KUNIT_CASE(dm_test_irq_unregister_rejects_null_handler),
	KUNIT_CASE(dm_test_irq_unregister_handler_not_found),
	/* amdgpu_dm_irq_fini */
	KUNIT_CASE(dm_test_irq_fini_removes_registered_handlers),
	KUNIT_CASE(dm_test_irq_fini_on_empty_tables),
	/* amdgpu_dm_get_crtc_by_otg_inst */
	KUNIT_CASE(dm_test_get_crtc_by_otg_inst_returns_match),
	KUNIT_CASE(dm_test_get_crtc_by_otg_inst_returns_null),
	KUNIT_CASE(dm_test_get_crtc_by_otg_inst_empty_list),
	{}
};

static struct kunit_suite amdgpu_dm_irq_test_suite = {
	.name = "amdgpu_dm_irq",
	.test_cases = amdgpu_dm_irq_tests,
};

kunit_test_suite(amdgpu_dm_irq_test_suite);

MODULE_AUTHOR("AMD");
MODULE_DESCRIPTION("KUnit tests for amdgpu_dm_irq");
MODULE_LICENSE("Dual MIT/GPL");
