/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_PM_RUNTIME_BACKPORT_H
#define AMDKCL_PM_RUNTIME_BACKPORT_H

#include <linux/version.h>
#include <linux/pm_runtime.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 18, 0)
#if !defined(HAVE_PM_RUNTIME_PUT_AUTOSUSPEND_MARK_LAST_BUSY)
static inline int _kcl_pm_runtime_put_autosuspend(struct device *dev)
{
	pm_runtime_mark_last_busy(dev);
	return pm_runtime_put_autosuspend(dev);
}
#define pm_runtime_put_autosuspend _kcl_pm_runtime_put_autosuspend
#endif
#endif

#endif
