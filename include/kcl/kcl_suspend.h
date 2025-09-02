/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_SUSPEND_H
#define AMDKCL_SUSPEND_H

#include <linux/suspend.h>

#ifdef CONFIG_PM_SLEEP
#ifndef HAVE_KSYS_SYNC_HELPER
extern void _kcl_ksys_sync_helper(void);

static inline void ksys_sync_helper(void)
{
	_kcl_ksys_sync_helper();
}
#endif /* HAVE_KSYS_SYNC_HELPER */
#else
#ifndef HAVE_KSYS_SYNC_HELPER
static inline void ksys_sync_helper(void) {}
#endif /* HAVE_KSYS_SYNC_HELPER */

#ifndef HAVE_PM_HIBERNATE_IS_RECOVERING
static inline bool pm_hibernate_is_recovering(void) { return false; }
#endif
#endif /* CONFIG_PM_SLEEP */

#ifndef HAVE_PM_SUSPEND_VIA_FIRMWARE
static inline bool pm_suspend_via_firmware(void) { return false; }

static inline bool pm_resume_via_firmware(void) { return false; }
#endif /* HAVE_PM_SUSPEND_VIA_FIRMWARE */

#endif /* AMDKCL_SUSPEND_H */
