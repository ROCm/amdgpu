// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * KUnit tests for aca_v5_0.c: attribution of MP5-aggregated GFX MCA banks.
 *
 * MP5 firmware copies a GFX MCA bank error into its own MCA bank (IPID
 * HWID 0x01 / ACAType 0x2), with STATUS.ErrorCodeExt 0x3E and
 * SYND.ErrorInformation[15:0] 0x18. The tests drive the GFX block's
 * bank_match/bank_parse through the exported ras_aca_func_v5_0 table.
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <kunit/test.h>
#include <linux/module.h>

#include "ras.h"
#include "aca.h"
#include "aca_v5_0.h"

/*
 * MP5 copy of a GFX correctable error as logged by the x86 MCE core on an
 * AMD Instinct MI450 system: STATUS Val|En|MiscV|AddrV|SyndV with
 * ErrorCodeExt 0x3E; IPID ACAType 0x2, InstanceIdHi (die) 1, HWID 0x01,
 * InstanceIdLo 0x36430404 (socket 4); SYND ErrorInformation 0x1_0018.
 */
#define MP5_AGGR_STATUS		0x9c200000003e0109ULL
#define MP5_AGGR_ADDR		0x000000000000002eULL
#define MP5_AGGR_MISC0		0xd018000000000000ULL	/* ErrCnt 0 */
#define MP5_AGGR_MISC0_ERRCNT3	0xd018000300000000ULL	/* ErrCnt 3 */
#define MP5_IPID		0x0002100136430404ULL
#define MP5_SYND_AGGR_ERR	0x0000000042010018ULL

/* MP5 self-logged errors: SMN timeout (0x2) and DMA error (0x19) */
#define MP5_SYND_SMN_TIMEOUT	0x0000000042010002ULL
#define MP5_SYND_DMA_ERR	0x0000000042010019ULL

/* MP5_AGGR_STATUS variants */
#define MP5_STATUS_EXT_3D	0x9c200000003d0109ULL	/* ErrorCodeExt 0x3D */
#define MP5_STATUS_NO_SYNDV	0x9c000000003e0109ULL	/* SyndV clear */
#define MP5_STATUS_DEFERRED	0x9c201000003e0109ULL	/* + Deferred */
#define MP5_STATUS_UC_PCC	0xbe200000003e0109ULL	/* + UC, PCC */
#define MP5_STATUS_TCC		0x9ca00000003e0109ULL	/* + TCC */

/* GFX XCD bank (HWID 0x2E2, ACAType 0x0) on the same socket and die */
#define GFX_XCD_IPID		0x000012e236430404ULL

struct aca_v5_0_test_ctx {
	struct ras_core_context *ras_core;
	struct aca_block *blk;
	struct aca_bank_reg bank;
	struct aca_bank_ecc ecc;
};

static const struct aca_block_info *aca_v5_0_test_find_block(struct kunit *test,
							     u32 ras_block_id)
{
	const struct ras_aca_ip_func *func = &ras_aca_func_v5_0;
	u32 i;

	for (i = 0; i < func->block_num; i++) {
		if (func->block_info[i]->ras_block_id == ras_block_id)
			return func->block_info[i];
	}

	KUNIT_FAIL(test, "no ACA v5.0 block for RAS block %u", ras_block_id);
	return NULL;
}

static int aca_v5_0_test_init(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ctx);

	ctx->ras_core = kunit_kzalloc(test, sizeof(*ctx->ras_core), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ctx->ras_core);

	ctx->blk = kunit_kzalloc(test, sizeof(*ctx->blk), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, ctx->blk);

	ctx->blk->blk_info = aca_v5_0_test_find_block(test, RAS_BLOCK_ID__GFX);
	KUNIT_ASSERT_NOT_NULL(test, ctx->blk->blk_info);

	test->priv = ctx;
	return 0;
}

static void aca_v5_0_test_set_bank(struct aca_v5_0_test_ctx *ctx, u32 ecc_type,
				   u64 status, u64 misc0, u64 ipid, u64 synd)
{
	memset(&ctx->bank, 0, sizeof(ctx->bank));
	ctx->bank.ecc_type = ecc_type;
	ctx->bank.regs[ACA_REG_IDX__STATUS] = status;
	ctx->bank.regs[ACA_REG_IDX__ADDR] = MP5_AGGR_ADDR;
	ctx->bank.regs[ACA_REG_IDX__MISC0] = misc0;
	ctx->bank.regs[ACA_REG_IDX__IPID] = ipid;
	ctx->bank.regs[ACA_REG_IDX__SYND] = synd;
}

static bool aca_v5_0_test_gfx_match(struct aca_v5_0_test_ctx *ctx)
{
	return ctx->blk->blk_info->bank_ops.bank_match(ctx->blk, &ctx->bank);
}

static int aca_v5_0_test_gfx_parse(struct aca_v5_0_test_ctx *ctx)
{
	memset(&ctx->ecc, 0xff, sizeof(ctx->ecc));
	return ctx->blk->blk_info->bank_ops.bank_parse(ctx->ras_core, ctx->blk,
						       &ctx->bank, &ctx->ecc);
}

static void aca_v5_0_gfx_match_mp5_aggr_ce(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_EXPECT_TRUE(test, aca_v5_0_test_gfx_match(ctx));
}

static void aca_v5_0_gfx_match_rejects_mp5_self_errors(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_SMN_TIMEOUT);
	KUNIT_EXPECT_FALSE(test, aca_v5_0_test_gfx_match(ctx));

	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_DMA_ERR);
	KUNIT_EXPECT_FALSE(test, aca_v5_0_test_gfx_match(ctx));
}

static void aca_v5_0_gfx_match_requires_aggr_markers(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	/* ErrorCodeExt is not FW-logged (0x3E) */
	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_STATUS_EXT_3D,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_EXPECT_FALSE(test, aca_v5_0_test_gfx_match(ctx));

	/* SYND not valid */
	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_STATUS_NO_SYNDV,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_EXPECT_FALSE(test, aca_v5_0_test_gfx_match(ctx));
}

static void aca_v5_0_mp5_aggr_matches_only_gfx(struct kunit *test)
{
	const struct ras_aca_ip_func *func = &ras_aca_func_v5_0;
	struct aca_v5_0_test_ctx *ctx = test->priv;
	struct aca_block *blk = ctx->blk;
	u32 i, matches = 0;

	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_AGGR_ERR);

	for (i = 0; i < func->block_num; i++) {
		blk->blk_info = func->block_info[i];
		if (!blk->blk_info->bank_ops.bank_match(blk, &ctx->bank))
			continue;

		matches++;
		KUNIT_EXPECT_EQ(test, blk->blk_info->ras_block_id, (u32)RAS_BLOCK_ID__GFX);
	}

	KUNIT_EXPECT_EQ(test, matches, 1U);
}

static void aca_v5_0_gfx_parse_mp5_ce_zero_errcnt(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);

	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 1U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.de_count, 0U);

	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.hwid, 0x01);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.mcatype, 0x02);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.socket_id, 4);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.die_id, 1);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.status, MP5_AGGR_STATUS);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.ipid, MP5_IPID);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.addr, MP5_AGGR_ADDR);
}

static void aca_v5_0_gfx_parse_mp5_ce_errcnt(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0_ERRCNT3, MP5_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);

	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 3U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.de_count, 0U);
}

static void aca_v5_0_gfx_parse_mp5_ue(struct kunit *test)
{
	static const u64 ue_status[] = { MP5_STATUS_UC_PCC, MP5_STATUS_TCC };
	struct aca_v5_0_test_ctx *ctx = test->priv;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ue_status); i++) {
		aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, ue_status[i],
				       MP5_AGGR_MISC0_ERRCNT3, MP5_IPID,
				       MP5_SYND_AGGR_ERR);
		KUNIT_EXPECT_TRUE(test, aca_v5_0_test_gfx_match(ctx));
		KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);

		KUNIT_EXPECT_EQ_MSG(test, ctx->ecc.ue_count, 1U,
				    "STATUS 0x%016llx", ue_status[i]);
		KUNIT_EXPECT_EQ_MSG(test, ctx->ecc.ce_count, 0U,
				    "STATUS 0x%016llx", ue_status[i]);
		KUNIT_EXPECT_EQ_MSG(test, ctx->ecc.de_count, 0U,
				    "STATUS 0x%016llx", ue_status[i]);
	}
}

static void aca_v5_0_gfx_parse_mp5_deferred(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	/* A deferred copy keeps the default parser's DE result */
	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_STATUS_DEFERRED,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_EXPECT_TRUE(test, aca_v5_0_test_gfx_match(ctx));
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);

	KUNIT_EXPECT_EQ(test, ctx->ecc.de_count, 1U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 0U);
}

static void aca_v5_0_gfx_parse_mp5_not_mce(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	/* Only copies delivered through the MCE notifier are reclassified */
	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__CE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0, MP5_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);

	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.de_count, 0U);
}

static void aca_v5_0_gfx_xcd_bank_default_path(struct kunit *test)
{
	struct aca_v5_0_test_ctx *ctx = test->priv;

	/* A GFX XCD bank still matches through the HWID/ACAType map */
	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0, GFX_XCD_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_EXPECT_TRUE(test, aca_v5_0_test_gfx_match(ctx));

	/* MCE CE count is MISC0.ErrCnt as before, even when it is 0 */
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.de_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.hwid, 0x2e2);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.mcatype, 0x00);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.socket_id, 4);
	KUNIT_EXPECT_EQ(test, ctx->ecc.bank_info.die_id, 1);

	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_AGGR_STATUS,
			       MP5_AGGR_MISC0_ERRCNT3, GFX_XCD_IPID,
			       MP5_SYND_AGGR_ERR);
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 3U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 0U);

	/* MCE UC status is not reclassified for a non-MP5 bank */
	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__MCE, MP5_STATUS_UC_PCC,
			       MP5_AGGR_MISC0, GFX_XCD_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 0U);

	/* A UE bank from the polling path is one UE */
	aca_v5_0_test_set_bank(ctx, RAS_ERR_TYPE__UE, MP5_STATUS_UC_PCC,
			       MP5_AGGR_MISC0, GFX_XCD_IPID, MP5_SYND_AGGR_ERR);
	KUNIT_ASSERT_EQ(test, aca_v5_0_test_gfx_parse(ctx), 0);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ue_count, 1U);
	KUNIT_EXPECT_EQ(test, ctx->ecc.ce_count, 0U);
}

static struct kunit_case aca_v5_0_test_cases[] = {
	KUNIT_CASE(aca_v5_0_gfx_match_mp5_aggr_ce),
	KUNIT_CASE(aca_v5_0_gfx_match_rejects_mp5_self_errors),
	KUNIT_CASE(aca_v5_0_gfx_match_requires_aggr_markers),
	KUNIT_CASE(aca_v5_0_mp5_aggr_matches_only_gfx),
	KUNIT_CASE(aca_v5_0_gfx_parse_mp5_ce_zero_errcnt),
	KUNIT_CASE(aca_v5_0_gfx_parse_mp5_ce_errcnt),
	KUNIT_CASE(aca_v5_0_gfx_parse_mp5_ue),
	KUNIT_CASE(aca_v5_0_gfx_parse_mp5_deferred),
	KUNIT_CASE(aca_v5_0_gfx_parse_mp5_not_mce),
	KUNIT_CASE(aca_v5_0_gfx_xcd_bank_default_path),
	{}
};

static struct kunit_suite aca_v5_0_test_suite = {
	.name = "amdgpu_ras_aca_v5_0",
	.init = aca_v5_0_test_init,
	.test_cases = aca_v5_0_test_cases,
};

kunit_test_suite(aca_v5_0_test_suite);

MODULE_IMPORT_NS("EXPORTED_FOR_KUNIT_TESTING");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("KUnit tests for the AMDGPU ACA v5.0 bank parser");
