// SPDX-License-Identifier: GPL-2.0-only
/*
 * kernel/power/main.c - PM subsystem core functionality.
 *
 * Copyright (c) 2003 Patrick Mochel
 * Copyright (c) 2003 Open Source Development Lab
 */
#include <linux/bug.h>
#include <kcl/kcl_suspend.h>

#ifndef HAVE_PM_HIBERNATE_IS_RECOVERING
static pm_message_t pm_transition;
#endif

#ifndef HAVE_KSYS_SYNC_HELPER
/* Copied from kernel/power/main.c */
#ifdef CONFIG_PM_SLEEP
long (*_kcl_ksys_sync)(void);

void _kcl_ksys_sync_helper(void)
{
	pr_info("Syncing filesystems ... ");
	_kcl_ksys_sync();
	pr_cont("done.\n");
}
EXPORT_SYMBOL(_kcl_ksys_sync_helper);

static bool _kcl_sys_sync_stub(void)
{
	pr_warn_once("kernel symbol [k]sys_sync not found!\n");
	return false;
}
#endif /* CONFIG_PM_SLEEP */
#endif /* HAVE_KSYS_SYNC_HELPER */

void amdkcl_suspend_init(void)
{
#ifndef HAVE_KSYS_SYNC_HELPER
#ifdef CONFIG_PM_SLEEP
	_kcl_ksys_sync = amdkcl_fp_setup("ksys_sync", _kcl_sys_sync_stub);
	if (_kcl_ksys_sync != _kcl_sys_sync_stub) {
		return;
	}

	_kcl_ksys_sync = amdkcl_fp_setup("sys_sync", _kcl_sys_sync_stub);
	if (_kcl_ksys_sync != _kcl_sys_sync_stub) {
		return;
	}

	pr_err_once("Error: fail to get symbol [k]sys_sync!\n");
	BUG();
#endif /* CONFIG_PM_SLEEP */
#endif /* HAVE_KSYS_SYNC_HELPER */
}

#ifndef HAVE_PM_HIBERNATE_IS_RECOVERING
/**
 * pm_hibernate_is_recovering - if recovering from hibernate due to error.
 *
 * Used to query if dev_pm_ops.thaw() is called for normal hibernation case or
 * recovering from some error.
 *
 * Return: true for error case, false for normal case.
 */
bool kcl_pm_hibernate_is_recovering(void)
{
	return pm_transition.event == PM_EVENT_RECOVER;
}
EXPORT_SYMBOL_GPL(kcl_pm_hibernate_is_recovering);
#endif
