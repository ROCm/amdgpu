/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 */
#ifndef _UFS_AMD_H
#define _UFS_AMD_H

#define REG_UFS_AMD_SQD         0x2200  /* SQ Doorbell base (runtime) */
#define REG_UFS_AMD_SQIS        0x2214  /* SQ Interrupt Status base */
#define REG_UFS_AMD_CQD         0x221C  /* CQ Doorbell base */
#define REG_UFS_AMD_CQIS        0x2224  /* CQ Interrupt Status base */
#define REG_UFS_AMD_MCQ_STRIDE  0x40    /* 64 bytes per queue */

int ufs_amd_mcq_config_resource(struct ufs_hba *hba);
int ufs_amd_op_runtime_config(struct ufs_hba *hba);

static struct ufs_hba_variant_ops ufs_amd_hba_vops = {
	.name                   = "amd-pci",
	.mcq_config_resource    = ufs_amd_mcq_config_resource,
	.op_runtime_config      = ufs_amd_op_runtime_config,
};

#endif /* !_UFS_AMD_H */
