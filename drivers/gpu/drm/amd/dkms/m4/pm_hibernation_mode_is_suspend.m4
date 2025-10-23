dnl #
dnl # v6.16-2076-gbed9bcd014c2
dnl # PM: hibernate: Add pm_hibernation_mode_is_suspend()
dnl #
AC_DEFUN([AC_AMDGPU_PM_HIBERNATION_MODE_IS_SUSPEND], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/suspend.h>
		], [
			pm_hibernation_mode_is_suspend();
		], [
			AC_DEFINE(HAVE_PM_HIBERNATION_MODE_IS_SUSPEND, 1,
			[pm_hibernation_mode_is_suspend() is available])
		])
	])
])
