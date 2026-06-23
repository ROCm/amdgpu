// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for amdgpu_dm_irq.c
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <drm/drm_kunit_helpers.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>

#include "dc.h"
#include "inc/core_types.h"
#include "irq/irq_service.h"
#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_irq.h"
#include "amdgpu_dm_kunit_test_helpers.h"
#include "dc_dmub_srv.h"
#include "ivsrcid/ivsrcid_vislands30.h"
#include "ivsrcid/dcn/irqsrcs_dcn_1_0.h"
#include "link_service.h"
#include "dmub/dmub_srv.h"
#include "dal_asic_id.h"

static void dm_test_irq_handler(void *arg)
{
}

static void dm_test_irq_handler_alt(void *arg)
{
}

static void dm_test_irq_handler_count(void *arg)
{
	int *count = arg;

	if (count)
		(*count)++;
}

static struct dc *dm_test_alloc_dc_with_ctx(struct kunit *test)
{
	struct dc_context *ctx;
	struct dc *dc;

	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dc);
	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	dc->ctx = ctx;
	ctx->dc = dc;

	return dc;
}

static enum dc_irq_source dm_test_to_dal_irq_source_dcn10(
		struct irq_service *irq_service,
		uint32_t src_id,
		uint32_t ext_id)
{
	switch (src_id) {
	case DCN_1_0__SRCID__DC_D1_OTG_VSTARTUP:
		return DC_IRQ_SOURCE_VBLANK1;
	case DCN_1_0__SRCID__OTG0_IHC_V_UPDATE_NO_LOCK_INTERRUPT:
		return DC_IRQ_SOURCE_VUPDATE1;
	case DCN_1_0__SRCID__HUBP0_FLIP_INTERRUPT:
		return DC_IRQ_SOURCE_PFLIP1;
	case DCN_1_0__SRCID__DMCUB_OUTBOX_LOW_PRIORITY_READY_INT:
		return DC_IRQ_SOURCE_DMCUB_OUTBOX;
	default:
		return DC_IRQ_SOURCE_INVALID;
	}
}

static const struct irq_service_funcs dm_test_irq_service_funcs_dcn10 = {
	.to_dal_irq_source = dm_test_to_dal_irq_source_dcn10
};

static bool dm_test_irq_src_set(struct irq_service *irq_service,
				const struct irq_source_info *info, bool enable)
{
	return true;
}

static bool dm_test_irq_src_ack(struct irq_service *irq_service,
				const struct irq_source_info *info)
{
	return true;
}

/* Per-source funcs let dc_interrupt_set() succeed without register access. */
static struct irq_source_info_funcs dm_test_irq_src_funcs = {
	.set = dm_test_irq_src_set,
	.ack = dm_test_irq_src_ack,
};

static struct dc *dm_test_alloc_dc_with_irq_service(struct kunit *test,
						    const struct irq_service_funcs *funcs)
{
	struct irq_source_info *info;
	struct resource_pool *res_pool;
	struct irq_service *irqs;
	struct dc *dc;
	int i;

	dc = dm_test_alloc_dc_with_ctx(test);
	res_pool = kunit_kzalloc(test, sizeof(*res_pool), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, res_pool);
	irqs = kunit_kzalloc(test, sizeof(*irqs), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, irqs);

	/*
	 * Populate the per-source info table so dc_interrupt_set()/_ack()
	 * succeed without touching hardware registers.
	 */
	info = kunit_kzalloc(test, sizeof(*info) * DAL_IRQ_SOURCES_NUMBER,
			     GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, info);
	for (i = 0; i < DAL_IRQ_SOURCES_NUMBER; i++)
		info[i].funcs = &dm_test_irq_src_funcs;

	irqs->funcs = funcs;
	irqs->info = info;
	res_pool->irqs = irqs;
	dc->res_pool = res_pool;

	return dc;
}

static void dm_test_crtc_list_del(void *data)
{
	struct amdgpu_crtc *acrtc = data;

	list_del_init(&acrtc->base.head);
}

struct dm_test_hpd_rx_wq_ctx {
	struct hpd_rx_irq_offload_work_queue *wq;
	int count;
};

static void dm_test_destroy_hpd_rx_wq(void *data)
{
	struct dm_test_hpd_rx_wq_ctx *ctx = data;
	int i;

	for (i = 0; i < ctx->count; i++)
		if (ctx->wq[i].wq)
			destroy_workqueue(ctx->wq[i].wq);
	kfree(ctx->wq);
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

	adev = dm_kunit_alloc_adev(test);
	drm = &adev->ddev;

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

	adev = dm_kunit_alloc_adev(test);
	drm = &adev->ddev;

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

	adev = dm_kunit_alloc_adev(test);

	KUNIT_EXPECT_NULL(test, amdgpu_dm_get_crtc_by_otg_inst(adev, 0));
}

/* Tests for amdgpu_dm_set_irq_funcs() */

/**
 * dm_test_set_irq_funcs - Test irq src funcs and counts are populated
 * @test: The KUnit test context
 */
static void dm_test_set_irq_funcs(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	adev->mode_info.num_crtc = 6;
	adev->mode_info.num_hpd = 4;

	amdgpu_dm_set_irq_funcs(adev);

	KUNIT_EXPECT_EQ(test, adev->crtc_irq.num_types, 6);
	KUNIT_EXPECT_EQ(test, adev->vline0_irq.num_types, 6);
	KUNIT_EXPECT_EQ(test, adev->vupdate_irq.num_types, 6);
	KUNIT_EXPECT_EQ(test, adev->pageflip_irq.num_types, 6);
	KUNIT_EXPECT_EQ(test, adev->dmub_outbox_irq.num_types, 1);
	KUNIT_EXPECT_EQ(test, adev->dmub_trace_irq.num_types, 1);
	KUNIT_EXPECT_EQ(test, adev->hpd_irq.num_types, 4);

	KUNIT_EXPECT_TRUE(test, adev->crtc_irq.funcs != NULL);
	KUNIT_EXPECT_TRUE(test, adev->vline0_irq.funcs != NULL);
	KUNIT_EXPECT_TRUE(test, adev->dmub_outbox_irq.funcs != NULL);
	KUNIT_EXPECT_TRUE(test, adev->vupdate_irq.funcs != NULL);
	KUNIT_EXPECT_TRUE(test, adev->dmub_trace_irq.funcs != NULL);
	KUNIT_EXPECT_TRUE(test, adev->pageflip_irq.funcs != NULL);
	KUNIT_EXPECT_TRUE(test, adev->hpd_irq.funcs != NULL);
}

/* Tests for amdgpu_dm_irq_suspend()/resume_early()/resume_late() */

/**
 * dm_test_irq_suspend_empty - Test suspend walks empty handler tables safely
 * @test: The KUnit test context
 */
static void dm_test_irq_suspend_empty(struct kunit *test)
{
	struct amdgpu_device *adev;
	int src;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	/*
	 * With no registered handlers the HW dc_interrupt_set() calls are
	 * skipped, so suspend must complete without touching the (absent) DC.
	 */
	amdgpu_dm_irq_suspend(adev);

	for (src = 0; src < DAL_IRQ_SOURCES_NUMBER; src++) {
		KUNIT_EXPECT_TRUE(test, list_empty(&adev->dm.irq_handler_list_low_tab[src]));
		KUNIT_EXPECT_TRUE(test, list_empty(&adev->dm.irq_handler_list_high_tab[src]));
	}
}

/**
 * dm_test_irq_resume_early_empty - Test early resume walks empty tables safely
 * @test: The KUnit test context
 */
static void dm_test_irq_resume_early_empty(struct kunit *test)
{
	struct amdgpu_device *adev;
	int src;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	amdgpu_dm_irq_resume_early(adev);

	for (src = 0; src < DAL_IRQ_SOURCES_NUMBER; src++)
		KUNIT_EXPECT_TRUE(test, list_empty(&adev->dm.irq_handler_list_high_tab[src]));
}

/**
 * dm_test_irq_resume_late_empty - Test late resume walks empty tables safely
 * @test: The KUnit test context
 */
static void dm_test_irq_resume_late_empty(struct kunit *test)
{
	struct amdgpu_device *adev;
	int src;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	amdgpu_dm_irq_resume_late(adev);

	for (src = 0; src < DAL_IRQ_SOURCES_NUMBER; src++)
		KUNIT_EXPECT_TRUE(test, list_empty(&adev->dm.irq_handler_list_low_tab[src]));
}

/**
 * dm_test_irq_suspend_registered - Test suspend reaches the dc_interrupt_set path
 * @test: The KUnit test context
 *
 * Registers a low-context HPD handler so the handler list is non-empty,
 * forcing amdgpu_dm_irq_suspend() to call dc_interrupt_set() (NULL-safe with
 * no DC) and flush_work() on the registered handler.
 */
static void dm_test_irq_suspend_registered(struct kunit *test)
{
	struct dc_interrupt_params int_params = { 0 };
	struct amdgpu_device *adev;
	void *handler;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	amdgpu_dm_irq_suspend(adev);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
					   dm_test_irq_handler);
}

/**
 * dm_test_irq_suspend_disables_polling - Test suspend disables KMS polling
 * @test: The KUnit test context
 *
 * With KMS polling active, suspend must take the poll-disable branch.
 * drm_kms_helper_poll_disable() only cancels the poll work; it leaves the
 * poll_enabled flag set (cleared later by drm_kms_helper_poll_fini()).
 */
static void dm_test_irq_suspend_disables_polling(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	/* Enable KMS polling so suspend takes the poll-disable branch. */
	drm_kms_helper_poll_init(&adev->ddev);
	KUNIT_ASSERT_TRUE(test, adev->ddev.mode_config.poll_enabled);

	amdgpu_dm_irq_suspend(adev);

	KUNIT_EXPECT_TRUE(test, adev->ddev.mode_config.poll_enabled);

	drm_kms_helper_poll_fini(&adev->ddev);
}

/**
 * dm_test_irq_resume_early_registered - Test early resume reaches dc_interrupt_set
 * @test: The KUnit test context
 *
 * Registers a low-context HPD RX handler so early resume calls
 * dc_interrupt_set() for the short-pulse interrupt source.
 */
static void dm_test_irq_resume_early_registered(struct kunit *test)
{
	struct dc_interrupt_params int_params = { 0 };
	struct amdgpu_device *adev;
	void *handler;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1RX;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	amdgpu_dm_irq_resume_early(adev);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1RX,
					   dm_test_irq_handler);
}

/**
 * dm_test_irq_resume_late_registered - Test late resume reaches dc_interrupt_set
 * @test: The KUnit test context
 *
 * Registers a low-context HPD handler so late resume calls dc_interrupt_set()
 * for the HPD interrupt source.
 */
static void dm_test_irq_resume_late_registered(struct kunit *test)
{
	struct dc_interrupt_params int_params = { 0 };
	struct amdgpu_device *adev;
	void *handler;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler, adev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	amdgpu_dm_irq_resume_late(adev);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1,
					   dm_test_irq_handler);
}

/**
 * dm_test_irq_resume_late_enables_polling - Test late resume re-enables polling
 * @test: The KUnit test context
 *
 * With KMS polling active, late resume must take the poll-enable branch and
 * leave polling enabled.
 */
static void dm_test_irq_resume_late_enables_polling(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = dm_kunit_alloc_adev(test);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	/* Enable KMS polling so resume_late takes the poll-enable branch. */
	drm_kms_helper_poll_init(&adev->ddev);
	KUNIT_ASSERT_TRUE(test, adev->ddev.mode_config.poll_enabled);

	amdgpu_dm_irq_resume_late(adev);

	KUNIT_EXPECT_TRUE(test, adev->ddev.mode_config.poll_enabled);

	drm_kms_helper_poll_fini(&adev->ddev);
}

/* Tests for amdgpu_dm_hpd_rx_irq_create_workqueue() */

/**
 * dm_test_hpd_rx_irq_create_workqueue - Test workqueue array creation
 * @test: The KUnit test context
 */
static void dm_test_hpd_rx_irq_create_workqueue(struct kunit *test)
{
	struct dm_test_hpd_rx_wq_ctx *ctx;
	struct amdgpu_device *adev;
	struct dc *dc;
	int i;

	adev = dm_kunit_alloc_adev(test);

	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dc);
	dc->caps.max_links = 4;
	adev->dm.dc = dc;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ctx->wq = amdgpu_dm_hpd_rx_irq_create_workqueue(adev);
	KUNIT_ASSERT_NOT_NULL(test, ctx->wq);
	ctx->count = dc->caps.max_links;
	KUNIT_ASSERT_EQ(test, kunit_add_action_or_reset(test, dm_test_destroy_hpd_rx_wq, ctx), 0);

	for (i = 0; i < dc->caps.max_links; i++)
		KUNIT_EXPECT_TRUE(test, ctx->wq[i].wq != NULL);
}

/* Tests for amdgpu_dm_hpd_rx_irq_work_suspend() */

/**
 * dm_test_hpd_rx_irq_work_suspend_null - Test suspend with no work queue
 * @test: The KUnit test context
 */
static void dm_test_hpd_rx_irq_work_suspend_null(struct kunit *test)
{
	struct amdgpu_display_manager *dm;

	dm = kunit_kzalloc(test, sizeof(*dm), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dm);

	/* A NULL hpd_rx_offload_wq must be a safe no-op (DC untouched). */
	amdgpu_dm_hpd_rx_irq_work_suspend(dm);
}

/**
 * dm_test_hpd_rx_irq_work_suspend_flushes - Test suspend flushes queues
 * @test: The KUnit test context
 */
static void dm_test_hpd_rx_irq_work_suspend_flushes(struct kunit *test)
{
	struct dm_test_hpd_rx_wq_ctx *ctx;
	struct amdgpu_device *adev;
	struct dc *dc;

	adev = dm_kunit_alloc_adev(test);

	dc = kunit_kzalloc(test, sizeof(*dc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dc);
	dc->caps.max_links = 2;
	adev->dm.dc = dc;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ctx->wq = amdgpu_dm_hpd_rx_irq_create_workqueue(adev);
	KUNIT_ASSERT_NOT_NULL(test, ctx->wq);
	ctx->count = dc->caps.max_links;
	KUNIT_ASSERT_EQ(test, kunit_add_action_or_reset(test, dm_test_destroy_hpd_rx_wq, ctx), 0);

	adev->dm.hpd_rx_offload_wq = ctx->wq;

	amdgpu_dm_hpd_rx_irq_work_suspend(&adev->dm);
}

/* Tests for CRTC-based irq state callbacks (no-CRTC early return) */

/**
 * dm_test_set_crtc_irq_state_no_crtc - Test crtc irq state with missing CRTC
 * @test: The KUnit test context
 */
static void dm_test_set_crtc_irq_state_no_crtc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	/* mode_info.crtcs[0] is NULL -> returns 0 without dereferencing DC. */
	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_crtc_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/**
 * dm_test_set_pflip_irq_state_no_crtc - Test pflip irq state with missing CRTC
 * @test: The KUnit test context
 */
static void dm_test_set_pflip_irq_state_no_crtc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_pflip_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_DISABLE), 0);
}

/**
 * dm_test_set_vline0_irq_state_no_crtc - Test vline0 irq state with missing CRTC
 * @test: The KUnit test context
 */
static void dm_test_set_vline0_irq_state_no_crtc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_vline0_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/**
 * dm_test_set_vupdate_irq_state_no_crtc - Test vupdate irq state with missing CRTC
 * @test: The KUnit test context
 */
static void dm_test_set_vupdate_irq_state_no_crtc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_vupdate_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/* Tests for CRTC-based irq state callbacks (dm_irq_state happy path) */

/**
 * dm_test_set_crtc_irq_state_otg_disabled - Test crtc irq state with disabled OTG
 * @test: The KUnit test context
 */
static void dm_test_set_crtc_irq_state_otg_disabled(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *acrtc;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	acrtc = kunit_kzalloc(test, sizeof(*acrtc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc);

	/* otg_inst == -1 short-circuits before computing the irq source. */
	acrtc->otg_inst = -1;
	adev->mode_info.crtcs[0] = acrtc;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_crtc_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/**
 * dm_test_set_crtc_irq_state_enable - Test crtc irq state reaches DC (enable)
 * @test: The KUnit test context
 */
static void dm_test_set_crtc_irq_state_enable(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *acrtc;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	acrtc = kunit_kzalloc(test, sizeof(*acrtc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc);

	/*
	 * otg_inst >= 0 computes the irq source and reaches the NULL-safe
	 * dc_interrupt_set(); the ips_support branch is skipped (dc == NULL).
	 */
	acrtc->otg_inst = 3;
	adev->mode_info.crtcs[0] = acrtc;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_crtc_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/**
 * dm_test_set_pflip_irq_state_disable - Test pflip irq state reaches DC (disable)
 * @test: The KUnit test context
 */
static void dm_test_set_pflip_irq_state_disable(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *acrtc;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	acrtc = kunit_kzalloc(test, sizeof(*acrtc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc);

	/* The disable state exercises the st == false path. */
	acrtc->otg_inst = 1;
	adev->mode_info.crtcs[0] = acrtc;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_pflip_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_DISABLE), 0);
}

/**
 * dm_test_set_vline0_irq_state_enable - Test vline0 irq state reaches DC (enable)
 * @test: The KUnit test context
 */
static void dm_test_set_vline0_irq_state_enable(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *acrtc;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	acrtc = kunit_kzalloc(test, sizeof(*acrtc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc);

	acrtc->otg_inst = 0;
	adev->mode_info.crtcs[0] = acrtc;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_vline0_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/**
 * dm_test_set_vupdate_irq_state_enable - Test vupdate irq state reaches DC (enable)
 * @test: The KUnit test context
 */
static void dm_test_set_vupdate_irq_state_enable(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *acrtc;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	acrtc = kunit_kzalloc(test, sizeof(*acrtc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc);

	acrtc->otg_inst = 2;
	adev->mode_info.crtcs[0] = acrtc;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_vupdate_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/**
 * dm_test_set_crtc_irq_state_allows_idle - Test the idle-optimization branch
 * @test: The KUnit test context
 *
 * With a non-NULL DC that advertises IPS support and currently allows idle
 * optimizations, dm_irq_state() must call dc_allow_idle_optimizations() before
 * dc_interrupt_set(). disable_idle_power_optimizations makes that call a safe
 * early return, and per-source stub funcs let dc_interrupt_set() succeed.
 */
static void dm_test_set_crtc_irq_state_allows_idle(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct amdgpu_crtc *acrtc;
	struct dal_logger *logger;
	struct dc *dc;

	adev = dm_kunit_alloc_adev(test);
	acrtc = kunit_kzalloc(test, sizeof(*acrtc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, acrtc);

	dc = dm_test_alloc_dc_with_irq_service(test, &dm_test_irq_service_funcs_dcn10);

	/* DC_LOG_* dereferences ctx->logger->dev, so wire a real drm device. */
	logger = kunit_kzalloc(test, sizeof(*logger), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, logger);
	logger->dev = &adev->ddev;
	dc->ctx->logger = logger;

	dc->caps.ips_support = true;
	dc->idle_optimizations_allowed = true;
	/* Keep dc_allow_idle_optimizations() a safe early return. */
	dc->debug.disable_idle_power_optimizations = true;
	adev->dm.dc = dc;

	acrtc->otg_inst = 0;
	adev->mode_info.crtcs[0] = acrtc;

	KUNIT_EXPECT_EQ(test,
			amdgpu_dm_set_crtc_irq_state(adev, NULL, 0, AMDGPU_IRQ_STATE_ENABLE), 0);
}

/* Tests for amdgpu_dm_irq_immediate_work() */

/**
 * dm_test_irq_immediate_work_empty - Test immediate work on empty high table
 * @test: The KUnit test context
 */
static void dm_test_irq_immediate_work_empty(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	/* No registered high-context handlers: must be a safe no-op. */
	amdgpu_dm_irq_immediate_work(adev, DC_IRQ_SOURCE_HPD1);
}

/**
 * dm_test_irq_immediate_work_invokes_handler - Test immediate work calls handler
 * @test: The KUnit test context
 */
static void dm_test_irq_immediate_work_invokes_handler(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	int count = 0;
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_HIGH_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler_count, &count);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	/* High-context handlers are invoked synchronously, in-place. */
	amdgpu_dm_irq_immediate_work(adev, DC_IRQ_SOURCE_HPD1);
	KUNIT_EXPECT_EQ(test, count, 1);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD1, dm_test_irq_handler_count);
}

/**
 * dm_test_irq_immediate_work_invokes_all - Test immediate work calls all handlers
 * @test: The KUnit test context
 */
static void dm_test_irq_immediate_work_invokes_all(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	int count = 0;
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_HIGH_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD2;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler_count, &count);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler_count, &count);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	/* Both registered high-context handlers must run. */
	amdgpu_dm_irq_immediate_work(adev, DC_IRQ_SOURCE_HPD2);
	KUNIT_EXPECT_EQ(test, count, 2);

	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD2, dm_test_irq_handler_count);
	amdgpu_dm_irq_unregister_interrupt(adev, DC_IRQ_SOURCE_HPD2, dm_test_irq_handler_count);
}

/* Tests for amdgpu_dm_irq_schedule_work() */

/**
 * dm_test_irq_schedule_work_empty - Test schedule work on empty low table
 * @test: The KUnit test context
 */
static void dm_test_irq_schedule_work_empty(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	/* Empty handler list: schedule_work returns immediately. */
	amdgpu_dm_irq_schedule_work(adev, DC_IRQ_SOURCE_HPD1);
}

/**
 * dm_test_irq_schedule_work_queues_handler - Test schedule work runs handler
 * @test: The KUnit test context
 */
static void dm_test_irq_schedule_work_queues_handler(struct kunit *test)
{
	struct amdgpu_device *adev;
	struct dc_interrupt_params int_params = { 0 };
	int count = 0;
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler_count, &count);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	amdgpu_dm_irq_schedule_work(adev, DC_IRQ_SOURCE_HPD1);

	/*
	 * Low-context work runs asynchronously on system_highpri_wq.
	 * amdgpu_dm_irq_fini() flushes each pending work item before freeing
	 * the handlers, so the handler is guaranteed to have run afterwards.
	 */
	amdgpu_dm_irq_fini(adev);
	KUNIT_EXPECT_EQ(test, count, 1);
}

/**
 * dm_test_irq_schedule_work_requeue_fallback - Test the re-queue fallback path
 * @test: The KUnit test context
 *
 * The first schedule queues the handler's work item. Issuing a second
 * schedule before the work has run makes queue_work() fail for the
 * still-pending item, forcing amdgpu_dm_irq_schedule_work() into the fallback
 * that allocates and queues a fresh handler copy. Both work items run when
 * amdgpu_dm_irq_fini() flushes the queue, so the handler fires twice.
 */
static void dm_test_irq_schedule_work_requeue_fallback(struct kunit *test)
{
	struct dc_interrupt_params int_params = { 0 };
	struct amdgpu_device *adev;
	int count = 0;
	void *handler;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);
	KUNIT_ASSERT_EQ(test, amdgpu_dm_irq_init(adev), 0);

	int_params.int_context = INTERRUPT_LOW_IRQ_CONTEXT;
	int_params.irq_source = DC_IRQ_SOURCE_HPD1;
	handler = amdgpu_dm_irq_register_interrupt(adev, &int_params,
						   dm_test_irq_handler_count, &count);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	amdgpu_dm_irq_schedule_work(adev, DC_IRQ_SOURCE_HPD1);
	amdgpu_dm_irq_schedule_work(adev, DC_IRQ_SOURCE_HPD1);

	amdgpu_dm_irq_fini(adev);
	KUNIT_EXPECT_EQ(test, count, 2);
}

/* Tests for amdgpu_dm_set_hpd_irq_state() */

/**
 * dm_test_set_hpd_irq_state_null_dc - Test HPD irq state with no DC
 * @test: The KUnit test context
 */
static void dm_test_set_hpd_irq_state_null_dc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	/* dc_interrupt_set() is a no-op when dc is NULL, so both states
	 * return 0 without dereferencing the (absent) DC.
	 */
	KUNIT_EXPECT_EQ(test, amdgpu_dm_set_hpd_irq_state(adev, NULL, AMDGPU_HPD_1,
							  AMDGPU_IRQ_STATE_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, amdgpu_dm_set_hpd_irq_state(adev, NULL, AMDGPU_HPD_1,
							  AMDGPU_IRQ_STATE_DISABLE), 0);
}

/* Tests for amdgpu_dm_set_dmub_outbox_irq_state() */

/**
 * dm_test_set_dmub_outbox_irq_state_null_dc - Test outbox irq state with no DC
 * @test: The KUnit test context
 */
static void dm_test_set_dmub_outbox_irq_state_null_dc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	KUNIT_EXPECT_EQ(test, amdgpu_dm_set_dmub_outbox_irq_state(adev, NULL, 0,
								  AMDGPU_IRQ_STATE_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, amdgpu_dm_set_dmub_outbox_irq_state(adev, NULL, 0,
								  AMDGPU_IRQ_STATE_DISABLE), 0);
}

/* Tests for amdgpu_dm_set_dmub_trace_irq_state() */

/**
 * dm_test_set_dmub_trace_irq_state_null_dc - Test trace irq state with no DC
 * @test: The KUnit test context
 */
static void dm_test_set_dmub_trace_irq_state_null_dc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	KUNIT_EXPECT_EQ(test, amdgpu_dm_set_dmub_trace_irq_state(adev, NULL, 0,
								 AMDGPU_IRQ_STATE_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, amdgpu_dm_set_dmub_trace_irq_state(adev, NULL, 0,
								 AMDGPU_IRQ_STATE_DISABLE), 0);
}

/* Tests for amdgpu_dm_outbox_init() */

/**
 * dm_test_outbox_init_null_dc - Test outbox init is a safe no-op with no DC
 * @test: The KUnit test context
 */
static void dm_test_outbox_init_null_dc(struct kunit *test)
{
	struct amdgpu_device *adev;

	adev = kunit_kzalloc(test, sizeof(*adev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, adev);

	/* Single dc_interrupt_set() call must be skipped when dc is NULL. */
	amdgpu_dm_outbox_init(adev);
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
	/* amdgpu_dm_set_irq_funcs */
	KUNIT_CASE(dm_test_set_irq_funcs),
	/* amdgpu_dm_irq_suspend/resume_early/resume_late */
	KUNIT_CASE(dm_test_irq_suspend_empty),
	KUNIT_CASE(dm_test_irq_resume_early_empty),
	KUNIT_CASE(dm_test_irq_resume_late_empty),
	KUNIT_CASE(dm_test_irq_suspend_registered),
	KUNIT_CASE(dm_test_irq_suspend_disables_polling),
	KUNIT_CASE(dm_test_irq_resume_early_registered),
	KUNIT_CASE(dm_test_irq_resume_late_registered),
	KUNIT_CASE(dm_test_irq_resume_late_enables_polling),
	/* amdgpu_dm_hpd_rx_irq_create_workqueue */
	KUNIT_CASE(dm_test_hpd_rx_irq_create_workqueue),
	/* amdgpu_dm_hpd_rx_irq_work_suspend */
	KUNIT_CASE(dm_test_hpd_rx_irq_work_suspend_null),
	KUNIT_CASE(dm_test_hpd_rx_irq_work_suspend_flushes),
	/* CRTC-based irq state callbacks (no-CRTC early return) */
	KUNIT_CASE(dm_test_set_crtc_irq_state_no_crtc),
	KUNIT_CASE(dm_test_set_pflip_irq_state_no_crtc),
	KUNIT_CASE(dm_test_set_vline0_irq_state_no_crtc),
	KUNIT_CASE(dm_test_set_vupdate_irq_state_no_crtc),
	/* CRTC-based irq state callbacks (dm_irq_state happy path) */
	KUNIT_CASE(dm_test_set_crtc_irq_state_otg_disabled),
	KUNIT_CASE(dm_test_set_crtc_irq_state_enable),
	KUNIT_CASE(dm_test_set_pflip_irq_state_disable),
	KUNIT_CASE(dm_test_set_vline0_irq_state_enable),
	KUNIT_CASE(dm_test_set_vupdate_irq_state_enable),
	KUNIT_CASE(dm_test_set_crtc_irq_state_allows_idle),
	/* amdgpu_dm_irq_immediate_work */
	KUNIT_CASE(dm_test_irq_immediate_work_empty),
	KUNIT_CASE(dm_test_irq_immediate_work_invokes_handler),
	KUNIT_CASE(dm_test_irq_immediate_work_invokes_all),
	/* amdgpu_dm_irq_schedule_work */
	KUNIT_CASE(dm_test_irq_schedule_work_empty),
	KUNIT_CASE(dm_test_irq_schedule_work_queues_handler),
	KUNIT_CASE(dm_test_irq_schedule_work_requeue_fallback),
	/* amdgpu_dm_set_hpd_irq_state */
	KUNIT_CASE(dm_test_set_hpd_irq_state_null_dc),
	/* amdgpu_dm_set_dmub_outbox_irq_state */
	KUNIT_CASE(dm_test_set_dmub_outbox_irq_state_null_dc),
	/* amdgpu_dm_set_dmub_trace_irq_state */
	KUNIT_CASE(dm_test_set_dmub_trace_irq_state_null_dc),
	/* amdgpu_dm_outbox_init */
	KUNIT_CASE(dm_test_outbox_init_null_dc),
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
