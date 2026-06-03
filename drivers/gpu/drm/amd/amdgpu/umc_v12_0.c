/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "umc_v12_0.h"
#include "amdgpu_ras.h"
#include "amdgpu_umc.h"
#include "amdgpu.h"
#include "umc/umc_12_0_0_offset.h"
#include "umc/umc_12_0_0_sh_mask.h"
#include "mp/mp_13_0_6_sh_mask.h"

bool umc_v12_0_is_deferred_error(struct amdgpu_device *adev, uint64_t mc_umc_status)
{
	dev_dbg(adev->dev,
		"MCA_UMC_STATUS(0x%llx): Val:%llu, Poison:%llu, Deferred:%llu, PCC:%llu, UC:%llu, TCC:%llu\n",
		mc_umc_status,
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val),
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Poison),
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Deferred),
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, PCC),
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UC),
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, TCC)
	);

	return (amdgpu_ras_is_poison_mode_supported(adev) &&
		(REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1) &&
		((REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Deferred) == 1) ||
		(REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Poison) == 1)));
}

bool umc_v12_0_is_uncorrectable_error(struct amdgpu_device *adev, uint64_t mc_umc_status)
{
	if (umc_v12_0_is_deferred_error(adev, mc_umc_status))
		return false;

	return ((REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1) &&
		(REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, PCC) == 1 ||
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UC) == 1 ||
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, TCC) == 1));
}

bool umc_v12_0_is_correctable_error(struct amdgpu_device *adev, uint64_t mc_umc_status)
{
	if (umc_v12_0_is_deferred_error(adev, mc_umc_status))
		return false;

	return (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1 &&
		(REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, CECC) == 1 ||
		(REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UECC) == 1 &&
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UC) == 0) ||
		/* Identify data parity error in replay mode */
		((REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, ErrorCodeExt) == 0x5 ||
		REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, ErrorCodeExt) == 0xb) &&
		!(umc_v12_0_is_uncorrectable_error(adev, mc_umc_status)))));
}

static void umc_v12_0_get_retire_flip_bits(struct amdgpu_device *adev)
{
	enum amdgpu_memory_partition nps = AMDGPU_NPS1_PARTITION_MODE;
	uint32_t vram_type = adev->gmc.vram_type;
	struct amdgpu_umc_flip_bits *flip_bits = &(adev->umc.flip_bits);

	if (adev->gmc.gmc_funcs->query_mem_partition_mode)
		nps = adev->gmc.gmc_funcs->query_mem_partition_mode(adev);

	if (adev->gmc.num_umc == 16) {
		/* default setting */
		flip_bits->flip_bits_in_pa[0] = UMC_V12_0_PA_C2_BIT;
		flip_bits->flip_bits_in_pa[1] = UMC_V12_0_PA_C3_BIT;
		flip_bits->flip_bits_in_pa[2] = UMC_V12_0_PA_C4_BIT;
		flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R13_BIT;
		flip_bits->flip_row_bit = 13;
		flip_bits->bit_num = 4;
		flip_bits->r13_in_pa = UMC_V12_0_PA_R13_BIT;

		if (nps == AMDGPU_NPS2_PARTITION_MODE) {
			flip_bits->flip_bits_in_pa[0] = UMC_V12_0_PA_CH5_BIT;
			flip_bits->flip_bits_in_pa[1] = UMC_V12_0_PA_C2_BIT;
			flip_bits->flip_bits_in_pa[2] = UMC_V12_0_PA_B1_BIT;
			flip_bits->r13_in_pa = UMC_V12_0_PA_R12_BIT;
		} else if (nps == AMDGPU_NPS4_PARTITION_MODE) {
			flip_bits->flip_bits_in_pa[0] = UMC_V12_0_PA_CH4_BIT;
			flip_bits->flip_bits_in_pa[1] = UMC_V12_0_PA_CH5_BIT;
			flip_bits->flip_bits_in_pa[2] = UMC_V12_0_PA_B0_BIT;
			flip_bits->r13_in_pa = UMC_V12_0_PA_R11_BIT;
		}

		switch (vram_type) {
		case AMDGPU_VRAM_TYPE_HBM:
			/* other nps modes are taken as nps1 */
			if (nps == AMDGPU_NPS2_PARTITION_MODE)
				flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R12_BIT;
			else if (nps == AMDGPU_NPS4_PARTITION_MODE)
				flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R11_BIT;

			break;
		case AMDGPU_VRAM_TYPE_HBM3E:
			flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R12_BIT;
			flip_bits->flip_row_bit = 12;

			if (nps == AMDGPU_NPS2_PARTITION_MODE)
				flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R11_BIT;
			else if (nps == AMDGPU_NPS4_PARTITION_MODE)
				flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R10_BIT;

			break;
		default:
			dev_warn(adev->dev,
				"Unknown HBM type, set RAS retire flip bits to the value in NPS1 mode.\n");
			break;
		}
	} else if (adev->gmc.num_umc == 8) {
		/* default setting */
		flip_bits->flip_bits_in_pa[0] = UMC_V12_0_PA_CH5_BIT;
		flip_bits->flip_bits_in_pa[1] = UMC_V12_0_PA_C2_BIT;
		flip_bits->flip_bits_in_pa[2] = UMC_V12_0_PA_B1_BIT;
		flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R11_BIT;
		flip_bits->flip_row_bit = 12;
		flip_bits->bit_num = 4;
		flip_bits->r13_in_pa = UMC_V12_0_PA_R12_BIT;

		if (nps == AMDGPU_NPS2_PARTITION_MODE) {
			flip_bits->flip_bits_in_pa[0] = UMC_V12_0_PA_CH4_BIT;
			flip_bits->flip_bits_in_pa[1] = UMC_V12_0_PA_CH5_BIT;
			flip_bits->flip_bits_in_pa[2] = UMC_V12_0_PA_B0_BIT;
			flip_bits->r13_in_pa = UMC_V12_0_PA_R11_BIT;
		}

		switch (vram_type) {
		case AMDGPU_VRAM_TYPE_HBM:
			flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R12_BIT;

			/* other nps modes are taken as nps1 */
			if (nps == AMDGPU_NPS2_PARTITION_MODE)
				flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R11_BIT;

			break;
		case AMDGPU_VRAM_TYPE_HBM3E:
			flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R11_BIT;
			flip_bits->flip_row_bit = 12;

			if (nps == AMDGPU_NPS2_PARTITION_MODE)
				flip_bits->flip_bits_in_pa[3] = UMC_V12_0_PA_R10_BIT;

			break;
		default:
			dev_warn(adev->dev,
				"Unknown HBM type, set RAS retire flip bits to the value in NPS1 mode.\n");
			break;
		}
	} else {
		dev_warn(adev->dev,
			"Unsupported UMC number(%d), failed to set RAS flip bits.\n",
			adev->gmc.num_umc);

		return;
	}

	adev->umc.retire_unit = 0x1 << flip_bits->bit_num;
}

static bool umc_v12_0_check_ecc_err_status(struct amdgpu_device *adev,
			enum amdgpu_mca_error_type type, void *ras_error_status)
{
	uint64_t mc_umc_status = *(uint64_t *)ras_error_status;

	switch (type) {
	case AMDGPU_MCA_ERROR_TYPE_UE:
		return umc_v12_0_is_uncorrectable_error(adev, mc_umc_status);
	case AMDGPU_MCA_ERROR_TYPE_CE:
		return umc_v12_0_is_correctable_error(adev, mc_umc_status);
	case AMDGPU_MCA_ERROR_TYPE_DE:
		return umc_v12_0_is_deferred_error(adev, mc_umc_status);
	default:
		return false;
	}

	return false;
}

static uint32_t umc_v12_0_get_die_id(struct amdgpu_device *adev,
		uint64_t mca_addr, uint64_t retired_page)
{
	uint32_t die = 0;

	/* we only calculate die id for nps1 mode right now */
	die += ((((retired_page >> 12) & 0x1ULL)^
	    ((retired_page >> 20) & 0x1ULL) ^
	    ((retired_page >> 27) & 0x1ULL) ^
	    ((retired_page >> 34) & 0x1ULL) ^
	    ((retired_page >> 41) & 0x1ULL)) << 0);

	/* the original PA_C4 and PA_R13 may be cleared in retired_page, so
	 * get them from mca_addr.
	 */
	die += ((((retired_page >> 13) & 0x1ULL) ^
	    ((mca_addr >> 5) & 0x1ULL) ^
	    ((retired_page >> 28) & 0x1ULL) ^
	    ((mca_addr >> 23) & 0x1ULL) ^
	    ((retired_page >> 42) & 0x1ULL)) << 1);
	die &= 3;

	return die;
}

static void umc_v12_0_mca_ipid_parse(struct amdgpu_device *adev, uint64_t ipid,
		uint32_t *did, uint32_t *ch, uint32_t *umc_inst, uint32_t *sid)
{
	if (did)
		*did = MCA_IPID_2_DIE_ID(ipid);
	if (ch)
		*ch = MCA_IPID_2_UMC_CH(ipid);
	if (umc_inst)
		*umc_inst = MCA_IPID_2_UMC_INST(ipid);
	if (sid)
		*sid = MCA_IPID_2_SOCKET_ID(ipid);
}

struct amdgpu_umc_ras umc_v12_0_ras = {
	.ras_block = {
		.hw_ops = NULL,
	},
	.check_ecc_err_status = umc_v12_0_check_ecc_err_status,
	.get_die_id_from_pa = umc_v12_0_get_die_id,
	.get_retire_flip_bits = umc_v12_0_get_retire_flip_bits,
	.mca_ipid_parse = umc_v12_0_mca_ipid_parse,
};

