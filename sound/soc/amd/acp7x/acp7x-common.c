// SPDX-License-Identifier: GPL-2.0-only
/*
 * AMD ACP PCI driver callback routines for ACP7.x
 * platforms.
 *
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/types.h>

#include "acp7x.h"

static int acp7x_power_on(void __iomem *acp_base)
{
	u32 val = 0;

	val = readl(acp_base + ACP_PGFSM_STATUS);
	if (!(val & ACP7X_PGFSM_STATUS_MASK))
		return 0;

	writel(ACP7X_PGFSM_CNTL_POWER_ON_MASK, acp_base + ACP_PGFSM_CONTROL);
	val = readl(acp_base + ACP_PGFSM_CONTROL);
	return readl_poll_timeout(acp_base + ACP_PGFSM_STATUS, val,
				  ((val & ACP7X_PGFSM_STATUS_MASK) == 0), DELAY_US, ACP7X_TIMEOUT);
}

static int acp7x_reset(void __iomem *acp_base)
{
	u32 val;
	int ret;

	writel(1, acp_base + ACP_SOFT_RESET);
	ret = readl_poll_timeout(acp_base + ACP_SOFT_RESET, val,
				 val & ACP_SOFT_RESET_SOFTRESET_AUDDONE_MASK,
				 DELAY_US, ACP7X_TIMEOUT);
	if (ret)
		return ret;

	writel(0, acp_base + ACP_SOFT_RESET);
	return readl_poll_timeout(acp_base + ACP_SOFT_RESET, val, !val, DELAY_US, ACP7X_TIMEOUT);
}

static int acp7x_init(void __iomem *acp_base, struct device *dev)
{
	int ret;

	ret = acp7x_power_on(acp_base);
	if (ret) {
		dev_err(dev, "ACP power on failed\n");
		return ret;
	}
	writel(0x01, acp_base + ACP_CONTROL);
	ret = acp7x_reset(acp_base);
	if (ret) {
		dev_err(dev, "ACP reset failed\n");
		return ret;
	}
	writel(0, acp_base + ACP_ZSC_DSP_CTRL);
	return 0;
}

static int acp7x_deinit(void __iomem *acp_base, struct device *dev)
{
	int ret;

	ret = acp7x_reset(acp_base);
	if (ret) {
		dev_err(dev, "ACP reset failed\n");
		return ret;
	}
	writel(0x01, acp_base + ACP_ZSC_DSP_CTRL);
	return 0;
}

void acp7x_hw_init_ops(struct acp_hw_ops *hw_ops)
{
	hw_ops->acp_init = acp7x_init;
	hw_ops->acp_deinit = acp7x_deinit;
}
