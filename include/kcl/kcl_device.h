/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for the NVM Express interface
 * Copyright (c) 2011-2014, Intel Corporation.
 */
#ifndef AMDKCL_DEVICE_H
#define AMDKCL_DEVICE_H

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ratelimit.h>

/* Copied from include/linux/dev_printk.h */
#if !defined(dev_err_once)
#ifdef CONFIG_PRINTK
#define dev_level_once(dev_level, dev, fmt, ...)			\
do {									\
	static bool __print_once __read_mostly;				\
									\
	if (!__print_once) {						\
		__print_once = true;					\
		dev_level(dev, fmt, ##__VA_ARGS__);			\
	}								\
} while (0)
#else
#define dev_level_once(dev_level, dev, fmt, ...)			\
do {									\
	if (0)								\
		dev_level(dev, fmt, ##__VA_ARGS__);			\
} while (0)
#endif

#define dev_err_once(dev, fmt, ...)					\
	dev_level_once(dev_err, dev, fmt, ##__VA_ARGS__)
#endif

#if !defined(dev_err_ratelimited)
#define dev_level_ratelimited(dev_level, dev, fmt, ...)			\
do {									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      DEFAULT_RATELIMIT_BURST);		\
	if (__ratelimit(&_rs))						\
		dev_level(dev, fmt, ##__VA_ARGS__);			\
} while (0)

#define dev_err_ratelimited(dev, fmt, ...)				\
	dev_level_ratelimited(dev_err, dev, fmt, ##__VA_ARGS__)
#endif

#ifndef HAVE_DEV_IS_REMOVABLE
static const u16 kcl_removable_pci_vendor_ids[] = {
	PCI_VENDOR_ID_ASMEDIA,
};

static inline bool _kcl_dev_is_removable(struct device *dev)
{
	struct pci_dev *pdev;
	unsigned int i;

	if (dev->bus != &pci_bus_type)
		return false;

	pdev = to_pci_dev(dev);
	while ((pdev = pci_upstream_bridge(pdev))) {
		for (i = 0; i < ARRAY_SIZE(kcl_removable_pci_vendor_ids);
		     i++) {
			if (pdev->vendor == kcl_removable_pci_vendor_ids[i])
				return true;
		}
	}

	return false;
}
#define dev_is_removable _kcl_dev_is_removable
#endif

#endif /* AMDKCL_DEVICE_H */
