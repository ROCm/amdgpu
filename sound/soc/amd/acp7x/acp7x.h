/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AMD Common ACP header file for ACP7.X variants(ACP7.D/7.E/7.F)
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef __SOUND_SOC_AMD_ACP7X_H
#define __SOUND_SOC_AMD_ACP7X_H

#include <linux/io.h>
#include <linux/pci.h>
#include <linux/types.h>

#include <sound/acp7x_chip_offset_byte.h>

#define ACP_DEVICE_ID		0x15E2
#define ACP7X_REG_START		0x1240000
#define ACP7X_REG_END		0x125C000

#define ACP7D_PCI_REV		0x7D
#define ACP7E_PCI_REV		0x7E
#define ACP7F_PCI_REV		0x7F

int snd_amd_acp_find_config(struct pci_dev *pci);

struct acp7x_dev_data {
	void __iomem *acp7x_base;
};

#endif /* __SOUND_SOC_AMD_ACP7X_H */
