/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_PM_RUNTIME_BACKPORT_H
#define AMDKCL_PM_RUNTIME_BACKPORT_H

#include <linux/version.h>
#include <linux/pm_runtime.h>
#include <kcl/kcl_pm_runtime.h>
#include <kcl/kcl_compiler_attributes.h>

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

#ifndef HAVE_PM_RUNTIME_ACQUIRE
/*
 * Open-code the pm_runtime_active_auto_try_enabled guard with __cleanup
 * instead of backporting DEFINE_GUARD_COND()/ACQUIRE(): acquire with
 * pm_runtime_resume_and_get(), release with pm_runtime_put_autosuspend()
 * only when the acquire succeeded.
 *
 * Kept here rather than in kcl/kcl_pm_runtime.h so the destructor picks up
 * the pm_runtime_put_autosuspend() override above.
 */
struct _kcl_pm_runtime_acquire {
	struct device *dev;
	int err;
};

static inline struct _kcl_pm_runtime_acquire
_kcl_pm_runtime_acquire_if_enabled_autosuspend(struct device *dev)
{
	struct _kcl_pm_runtime_acquire acquire = { .dev = dev };

	acquire.err = pm_runtime_resume_and_get(dev);

	return acquire;
}

static inline void
_kcl_pm_runtime_acquire_destructor(struct _kcl_pm_runtime_acquire *acquire)
{
	if (!acquire->err)
		pm_runtime_put_autosuspend(acquire->dev);
}

#define PM_RUNTIME_ACQUIRE_IF_ENABLED_AUTOSUSPEND(_dev, _var)		\
	struct _kcl_pm_runtime_acquire _var				\
		__cleanup(_kcl_pm_runtime_acquire_destructor) =		\
		_kcl_pm_runtime_acquire_if_enabled_autosuspend(_dev)

#define PM_RUNTIME_ACQUIRE_ERR(_var_ptr)	((_var_ptr)->err)
#endif /* HAVE_PM_RUNTIME_ACQUIRE */

#endif
