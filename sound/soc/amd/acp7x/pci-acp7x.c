// SPDX-License-Identifier: GPL-2.0-only
/*
 * AMD common ACP PCI driver for ACP7.x variants
 * which includes ACP7.D/7.E/7.F and future variants
 * with same register layout.
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "acp7x.h"

static int snd_acp7x_probe(struct pci_dev *pci,
			   const struct pci_device_id *pci_id)
{
	struct acp7x_dev_data *adata;
	u32 addr;
	u32 flag;
	int ret;

	flag = snd_amd_acp_find_config(pci);
	if (flag)
		return -ENODEV;

	/* ACP PCI revision id check for ACP7.x platforms */
	switch (pci->revision) {
	case ACP7D_PCI_REV:
	case ACP7E_PCI_REV:
	case ACP7F_PCI_REV:
		break;
	default:
		return -ENODEV;
	}
	if (pci_enable_device(pci)) {
		dev_err(&pci->dev, "pci_enable_device failed\n");
		return -ENODEV;
	}

	ret = pci_request_regions(pci, "AMD ACP7.x audio");
	if (ret < 0) {
		dev_err(&pci->dev, "pci_request_regions failed\n");
		goto disable_pci;
	}
	adata = devm_kzalloc(&pci->dev, sizeof(struct acp7x_dev_data),
			     GFP_KERNEL);
	if (!adata) {
		ret = -ENOMEM;
		goto release_regions;
	}
	addr = pci_resource_start(pci, 0);
	adata->acp7x_base = devm_ioremap(&pci->dev, addr,
					 pci_resource_len(pci, 0));
	if (!adata->acp7x_base) {
		ret = -ENOMEM;
		goto release_regions;
	}
	pci_set_master(pci);
	pci_set_drvdata(pci, adata);
	return 0;

release_regions:
	pci_release_regions(pci);
disable_pci:
	pci_disable_device(pci);

	return ret;
}

static void snd_acp7x_remove(struct pci_dev *pci)
{
	pci_release_regions(pci);
	pci_disable_device(pci);
}

static const struct pci_device_id snd_acp7x_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, ACP_DEVICE_ID),
	.class = PCI_CLASS_MULTIMEDIA_OTHER << 8,
	.class_mask = 0xffffff },
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, snd_acp7x_ids);

static struct pci_driver acp7x_pci_driver  = {
	.name = KBUILD_MODNAME,
	.id_table = snd_acp7x_ids,
	.probe = snd_acp7x_probe,
	.remove = snd_acp7x_remove,
};

module_pci_driver(acp7x_pci_driver);

MODULE_AUTHOR("Vijendar.Mukunda@amd.com");
MODULE_DESCRIPTION("AMD ACP PCI driver for ACP7.X");
MODULE_LICENSE("GPL");
